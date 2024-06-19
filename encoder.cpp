/* Sample implementation of encrypt engine + encoder. 
The input here is the raw camera stream (YUV420)  
which is encoded using H.264 and encrypted using AES.
AES encryption is inspired by https://github.com/saju/misc/blob/master/misc/openssl_aes.c */

#include "compml_video.hpp"
#include "codec.cpp"

/**
 * Create a 256 bit key and IV using the supplied key_data. salt can be added for taste.
 **/
int aes_init(unsigned char *key_data, int key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx)
{
  int i, nrounds = 5;
  unsigned char key[32], iv[32];
  
  /*
   * Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the supplied key material.
   * nrounds is the number of times the we hash the material. More rounds are more secure but
   * slower.
   */
  i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, key_data, key_data_len, nrounds, key, iv);
  if (i != 32) {
    printf("Key size is %d bits - should be 256 bits\n", i);
    return -1;
  }

  EVP_CIPHER_CTX_init(e_ctx);
  EVP_EncryptInit_ex(e_ctx, EVP_aes_256_cbc(), NULL, key, iv);
  
  return 0;
}

/*
 * Encrypt *len bytes of data
 * All data going in & out is considered binary (unsigned char[])
 */
unsigned char *aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext, int len)
{
  /* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes */
  int c_len = len + AES_BLOCK_SIZE, f_len = 0;
  unsigned char *ciphertext = (unsigned char *)malloc(c_len);

  /* allows reusing of 'e' for multiple encryption cycles */
  EVP_EncryptInit_ex(e, NULL, NULL, NULL, NULL);

  /* update ciphertext, c_len is filled with the length of ciphertext generated,
    *len is the size of plaintext in bytes */
  EVP_EncryptUpdate(e, ciphertext, &c_len, plaintext, len);

  /* update ciphertext with the final remaining bytes */
  EVP_EncryptFinal_ex(e, ciphertext+c_len, &f_len);

  len = c_len + f_len;
  return ciphertext;
}

int packet_count = 0;
static void encode_cloud(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   std::string pkt_dir, FILE* v_file, EVP_CIPHER_CTX* en)
{
    int ret;
    FILE* file;
    FILE* file2;
    char buf[1024];
    char buf2[1024];
    std::string enc;
    std::string plain;

    unsigned char *ciphertext;
    enc = pkt_dir + "/enc/";
    plain = pkt_dir + "/plain/";

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %ld\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    /* receive encoded packet from encoder */
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %ld (size=%5d)\n", pkt->pts, pkt->size);
        
        // save encrypted packets to directory
        snprintf(buf, sizeof(buf), "%s%07d", enc.c_str(), packet_count); // encrypted packets folder
        file = fopen(buf, "wb");
        ciphertext = aes_encrypt(en, (unsigned char*)pkt->data, pkt->size);
        fwrite(ciphertext, 1, pkt->size, file);           // Save encrypted packets
        
        // save unencrypted packets to directory
        snprintf(buf2, sizeof(buf2), "%s%07d", plain.c_str(), packet_count); // unencrypted packets folder
        file2 = fopen(buf2, "wb");
        fwrite(pkt->data, 1, pkt->size, file2);
        
        // Save locally to video
        saveVideo(pkt, v_file);
        packet_count++;

        fclose(file);
        fclose(file2);
        av_packet_unref(pkt);
    }
}

int main(int argc, char *argv[])
{
    int d = 0;
    double loop_time = 0;
    double t1 = 0;
    int ret;
    AVPacket *packet;
    AVFrame *frame;

    std::string outdir, logs, yuv_file, video_file_name; // video, Image Dir, csv file
    int num_frames;
    FILE *video_file, *ipstream, *log_stream;
    double elapsedTime;
    unsigned char *raw_img;

    /* AES encryption */ 
    EVP_CIPHER_CTX* en = EVP_CIPHER_CTX_new();
    unsigned int salt[] = {12345, 54321};
    unsigned char *key_data;
    int key_data_len;

    if (argc <= 2)
    {
        fprintf(stderr, "Usage: %s <path to yuv file> <number of frames to encode> \n"
                        "H.264 Encoder/Decoder used here.\n",
                argv[0]);
        exit(0);
    }

    outdir = "./outputs/";

    yuv_file = argv[1];             // YUV file
    num_frames = atoi(argv[2]);     // number of frames to encode

    Codec encoder(true, 100000, 100000);

    video_file_name = "./video.h264";

    video_file = fopen(video_file_name.c_str(), "wb");
    ipstream = fopen(yuv_file.c_str(), "r");

    int size = WAYMO_SIZE;
    raw_img = (unsigned char *)malloc(size);

    packet = allocatePacket(packet);
    frame = allocateFrame(frame);
    
    // Encryption
    key_data = (unsigned char*)"MyKey123";
    key_data_len = strlen((char*)key_data);

    /* gen key and iv. init the cipher ctx object */
    if (aes_init(key_data, key_data_len, (unsigned char *)&salt, en)) {
        printf("Couldn't initialize AES cipher\n");
        exit(1);
    }

    for (int i = 0; i < num_frames; i++)
    {
        /* Read frames from the YUV file */
        fread(raw_img, sizeof(unsigned char), size, ipstream);

        ret = av_image_fill_arrays(frame->data, frame->linesize, raw_img, AV_PIX_FMT_YUV420P, frame->width, frame->height, 1);
        if (ret < 0)
            exit(1);

        frame->pts = i;

        /* encode the image */
        encode_cloud(encoder.context, frame, packet, outdir, video_file, en);

    }

    encode_cloud(encoder.context, NULL, packet, outdir, video_file, en);
    
    fclose(video_file);
    fclose(ipstream);
    deallocateResources(packet, frame);
    EVP_CIPHER_CTX_free(en);

    return 0;
}
