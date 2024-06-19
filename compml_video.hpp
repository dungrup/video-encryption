#ifndef COMPML_VIDEO_HPP
#define COMPML_VIDEO_HPP

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <math.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include <thread>
#include <openssl/evp.h>
#include <openssl/aes.h>

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavutil/opt.h>
  #include <libavutil/imgutils.h>
  #include <libswscale/swscale.h>
}

#define WAYMO_WIDTH 1920
#define WAYMO_HEIGHT 1280
#define WAYMO_SIZE (1.5 * WAYMO_WIDTH * WAYMO_HEIGHT)

#endif