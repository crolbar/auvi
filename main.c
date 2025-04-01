#include "chuck_fft.h"
#include "string.h"
#include <math.h>
#include <raylib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "AL/al.h"
#include "AL/alc.h"

#define SAMPLE_RATE 10000
#define BUFFER_SIZE 256 // 256 // 512 // 1024 // Number of samples

struct auvi
{
    // sample amplitude scalar
    int amp_scalar;

    int avg_mode;
    int avg_size;

    // percentage of decay of amplitude in each frame
    int decay;

    float fft[BUFFER_SIZE];
    float fft_nrml[BUFFER_SIZE];

    ALCdevice* device;
    int device_idx;

    char** devices;
    size_t devices_size;

    int gui;
} typedef auvi;

volatile __sig_atomic_t keep_running = 1;

int
min(int x, int y)
{
    if (x > y) {
        return y;
    }
    return x;
}

int
max(int x, int y)
{
    if (x < y) {
        return y;
    }
    return x;
}

void
handle_sigint(int sig)
{
    keep_running = 0;
}

void
init_devices(auvi* a)
{
    ALCchar* devices;
    devices = (ALCchar*)alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);

    a->devices = NULL;
    a->devices_size = 0;

    int idx = 0;
    while (*devices) {
        char* dev_name = devices;

        // move to next
        devices += strlen(devices) + 1;

        if (a->devices_size <= idx) {
            a->devices = realloc(a->devices, idx + 1 * sizeof(char*));
            a->devices_size = a->devices_size + 1;
        }
        a->devices[idx] = dev_name;

        idx++;
    }
}

void
list_devices(auvi* a)
{
    for (int i = 0; i < a->devices_size; i++) {
        printf("%d: %s\n", i, a->devices[i]);
    }
}

void
init_device(auvi* a)
{

    ALCdevice* device = alcCaptureOpenDevice(
      a->devices[a->device_idx], SAMPLE_RATE, AL_FORMAT_MONO8, BUFFER_SIZE);

    alcCaptureStart(device);

    a->device = device;
}

void
setNormalization(auvi* a, float offset, float scale)
{
    for (int i = 0; i < BUFFER_SIZE; i++) {
        a->fft_nrml[i] = offset + (scale * (i / (float)BUFFER_SIZE));
    }
}

//
//
// BORROWED FROM KeyboardVisualizer
//
//
void
apply_fft(auvi* a, unsigned char sample_buf[BUFFER_SIZE])
{
    // tmp storage of fft on samples
    float fft_tmp[BUFFER_SIZE];

    // since the samples are u8 values, we shift them by 256/2 to the left
    // so we get a 0 when there is no sound at that time, instead of a 128
    //
    // and also scale the amps a bit for better visualization
    int shift = (float)(256.0f / 2);
    for (int i = 0; i < BUFFER_SIZE; i++) {
        fft_tmp[i] = (sample_buf[i] - shift) * ((float)a->amp_scalar / shift);
    }

    // run the fft
    rfft(fft_tmp, BUFFER_SIZE / 2, 1);

    // remove dc component
    fft_tmp[0] = fft_tmp[2];

    apply_window(fft_tmp, a->fft_nrml, BUFFER_SIZE);

    // fade out previous amps
    for (int i = 0; i < BUFFER_SIZE; i++) {
        a->fft[i] = a->fft[i] * (((float)a->decay) / 100.0f);
    }

    // Compute FFT magnitude
    for (int i = 0; i < BUFFER_SIZE / 2; i += 2) {
        float fftmag;

        // Compute magnitude from real and imaginary components of FFT and apply
        // simple LPF
        fftmag = (float)sqrt((fft_tmp[i] * fft_tmp[i]) +
                             (fft_tmp[i + 1] * fft_tmp[i + 1]));

        // Apply a slight logarithmic filter to minimize noise from very low
        // amplitude frequencies
        fftmag = (0.5f * log10(1.1f * fftmag)) + (0.9f * fftmag);

        // Limit FFT magnitude to 1.0
        if (fftmag > 1.0f) {
            fftmag = 1.0f;
        }

        // Update to new values only if greater than previous values
        if (fftmag > a->fft[i * 2]) {
            a->fft[i * 2] = fftmag;
        }

        // Prevent from going negative
        if (a->fft[i * 2] < 0.0f) {
            a->fft[i * 2] = 0.0f;
        }

        // Set odd indexes to match their corresponding even index, as the FFT
        // input array uses two indices for one value (real+imaginary)
        a->fft[(i * 2) + 1] = a->fft[i * 2];
        a->fft[(i * 2) + 2] = a->fft[i * 2];
        a->fft[(i * 2) + 3] = a->fft[i * 2];
    }

    if (a->avg_mode == 0) {
        // Apply averaging over given number of values
        int k;
        float sum1 = 0;
        float sum2 = 0;
        for (k = 0; k < a->avg_size; k++) {
            sum1 += a->fft[k];
            sum2 += a->fft[BUFFER_SIZE - 1 - k];
        }
        // Compute averages for end bars
        sum1 = sum1 / k;
        sum2 = sum2 / k;
        for (k = 0; k < a->avg_size; k++) {
            a->fft[k] = sum1;
            a->fft[BUFFER_SIZE - 1 - k] = sum2;
        }
        for (int i = 0; i < (BUFFER_SIZE - a->avg_size); i += a->avg_size) {
            float sum = 0;
            for (int j = 0; j < a->avg_size; j += 1) {
                sum += a->fft[i + j];
            }

            float avg = sum / a->avg_size;

            for (int j = 0; j < a->avg_size; j += 1) {
                a->fft[i + j] = avg;
            }
        }
    } else if (a->avg_mode == 1) {
        for (int i = 0; i < a->avg_size; i++) {
            float sum1 = 0;
            float sum2 = 0;
            int j;
            for (j = 0; j <= i + a->avg_size; j++) {
                sum1 += a->fft[j];
                sum2 += a->fft[BUFFER_SIZE - 1 - j];
            }
            a->fft[i] = sum1 / j;
            a->fft[BUFFER_SIZE - 1 - i] = sum2 / j;
        }
        for (int i = a->avg_size; i < BUFFER_SIZE - a->avg_size; i++) {
            float sum = 0;
            for (int j = 1; j <= a->avg_size; j++) {
                sum += a->fft[i - j];
                sum += a->fft[i + j];
            }
            sum += a->fft[i];

            a->fft[i] = sum / (2 * a->avg_size + 1);
        }
    }
}

void
update(auvi* a)
{
    float fft_buf[BUFFER_SIZE];
    ALCint samples;

    do {
        alcGetIntegerv(a->device, ALC_CAPTURE_SAMPLES, 1, &samples);
        usleep(1000);
    } while (samples < BUFFER_SIZE);

    unsigned char sample_buf[BUFFER_SIZE];
    alcCaptureSamples(a->device, (ALCvoid*)sample_buf, BUFFER_SIZE);

    apply_fft(a, sample_buf);
}

void
drawVisualizer(auvi* a)
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    Color color = (Color){ 200, 50, 50, 255 };

    float binWidth = (float)w / BUFFER_SIZE;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        int start_x = (int)(i * binWidth);
        int end_y = h - (h * a->fft[i]);

        if (end_y < 0)
            end_y = 0;

        if (end_y > h)
            end_y = h;

        int w = (int)binWidth;
        int next_bin_x = (int)((i + 1) * binWidth);
        int bin_end = start_x + binWidth;
        int rectWidth = (bin_end != next_bin_x)
                          ? (int)(binWidth) + (next_bin_x - bin_end)
                          : (int)(binWidth);

        DrawRectangle(start_x, end_y, rectWidth, h - end_y, color);
    }
}

int
main()
{
    signal(SIGINT, handle_sigint);
    signal(SIGKILL, handle_sigint);
    signal(SIGQUIT, handle_sigint);

    auvi a;
    a.gui = 1;
    a.device = NULL;
    a.devices = NULL;
    a.devices_size = 0;
    a.device_idx = 1;
    a.amp_scalar = 5000;
    a.avg_mode = 1;
    a.avg_size = 8;
    a.decay = 80;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        a.fft[i] = 0.0f;
    }

    init_devices(&a);
    if (a.devices_size == 0) {
        printf("no devices found\n");
        return 1;
    }

    list_devices(&a);

    init_device(&a);
    if (a.device == NULL) {
        printf("could not init device capture\n");
        return 1;
    }
    printf("using device: %s\n", a.devices[a.device_idx]);

    float nrml_ofst = 0.04f;
    float nrml_scl = 0.5f;
    setNormalization(&a, nrml_ofst, nrml_scl);

    if (a.gui) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(500, 400, "auvi");
        SetTargetFPS(144);
    }

    while (keep_running) {
        if (a.gui && WindowShouldClose()) {
            break;
        }
        update(&a);

        if (!a.gui) {
            continue;
        }

        BeginDrawing();
        ClearBackground((Color){ 20, 20, 20, 255 });

        drawVisualizer(&a);

        EndDrawing();
    }

    alcCaptureStop(a.device);
    alcCaptureCloseDevice(a.device);
    if (a.gui) {
        CloseWindow();
    }
    return 0;
}
