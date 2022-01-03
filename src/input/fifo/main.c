#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <time.h>

#include "shared.h"

int rc;

int open_fifo(const char *path) {
    int fd = open(path, O_RDONLY);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

// input: FIFO
EXP_FUNC void* xavaInput(void* data)
{
    XAVA_AUDIO *audio = (XAVA_AUDIO *)data;
    int fd;
    int n = 0;
    //signed char buf[1024];
    //int tempr, templ, lo, q;
    int i;
    int t = 0;
    //int size = 1024;
    ssize_t bytes = 0;
    int16_t buf[audio->inputsize];
    struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };

    fd = open_fifo(audio->source);

    // first read to remove any latency that may happen
    nanosleep (&req, NULL);
    while(!bytes) {
        bytes = read(fd, buf, sizeof(buf));
    }

    while (1) {
        bytes = read(fd, buf, sizeof(buf));

        if (bytes < 1) { //if no bytes read sleep 10ms and zero shared buffer
            nanosleep (&req, NULL);
            t++;
            if (t > 10) {
                for (i = 0; i < audio->inputsize; i++)
                    audio->audio_out_l[i] = 0;

                // prevents filling a invalid buffer whilst in mono mode
                if (audio->channels == 2) {
                    for (i = 0; i < audio->inputsize; i++)
                        audio->audio_out_r[i] = 0;
                }
                close(fd);
                fd = open_fifo(audio->source);
                t = 0;
            }
        } else { //if bytes read go ahead
            t = 0;

            // assuming samples are 16bit (as per example)
            // also reading more than the retrieved buffer is considered memory corruption
            for (i = 0; i < bytes/2; i += 2) {
                if (audio->channels == 1) audio->audio_out_l[n] = (float)(buf[i] + buf[i + 1]) / (float)2.0;

                //stereo storing channels in buffer
                if (audio->channels == 2) {
                    audio->audio_out_l[n] = buf[i];
                    audio->audio_out_r[n] = buf[i + 1];
                }

                n++;
                if (n == audio->inputsize-1) n = 0;
            }
        }

        if (audio->terminate == 1) {
            close(fd);
            break;
        }
    }
    return 0;
}

EXP_FUNC void xavaInputLoadConfig(XAVA *xava) {
    XAVA_AUDIO *audio = &xava->audio;
    xava_config_source config = xava->default_config.config;
    xavaWarnCondition(audio->rate != 44100,
            "Changing the audio rate won't do much as that depends on your MPD "
            "settings. Go check those instead!");
    audio->source = (char*)xavaConfigGetString(config, "input", "source", "/tmp/mpd.fifo");
}

