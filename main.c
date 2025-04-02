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

typedef enum avg_type
{
    // groups the frequencies as avg_size blocks
    // as the amp being the avg value of the frequencies
    // in that block
    Block,

    // smooths out amps as it takes the average value
    // of the neighboring frequencies amps and makes it
    // the result value of each frequency
    BoxFilter,

    // runs box filter twice
    DoubleBoxFilter,

    // box filter, the only difference being that
    // more distant frequencies from each frequency
    // contribute less to the avarage
    WeightedFilter,

    // smooths out the fft as it uses the `alpha` value
    // to control how much the neighboring (left/right)
    // frequencies contribute to the smoothing
    ExponentialFilter
} avg_type;

typedef struct auvi
{
    // sample amplitude scalar
    int amp_scalar;

    avg_type avg_mode;
    // block size, box / weighted filter range
    int avg_size;
    // used in ExponentialFilter
    float alpha;

    // percentage of decay of amplitude in each frame
    int decay;

    float fft[BUFFER_SIZE];

    ALCdevice* device;
    int device_idx;

    char** devices;
    size_t devices_size;

    int gui;
} auvi;

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

float
minf(float x, float y)
{
    if (x > y) {
        return y;
    }
    return x;
}

float
maxf(float x, float y)
{
    if (x < y) {
        return y;
    }
    return x;
}

float
clamp(float v, float low, float hight)
{
    return minf(hight, maxf(low, v));
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
apply_exponential_smoothing(float (*fft)[BUFFER_SIZE], float alpha)
{
    float tmp[BUFFER_SIZE];

    tmp[0] = (*fft)[0];
    for (int i = 1; i < BUFFER_SIZE; i++) {
        tmp[i] = alpha * (*fft)[i] + (1.0f - alpha) * tmp[i - 1];
    }

    (*fft)[BUFFER_SIZE - 1] = tmp[BUFFER_SIZE - 1];
    for (int i = BUFFER_SIZE - 2; i >= 0; i--) {
        (*fft)[i] = alpha * tmp[i] + (1.0f - alpha) * (*fft)[i + 1];
    }
}

void
apply_weighted_avg(float (*fft)[BUFFER_SIZE], int avg_size)
{
    float tmp[BUFFER_SIZE];

    for (int i = 0; i < BUFFER_SIZE; i++) {
        float sum = 0;
        float weight_sum = 0;

        int start = max(i - avg_size, 0);
        int end = min(i + avg_size, BUFFER_SIZE - 1);
        for (int j = start; j <= end; j++) {
            // decreases the influence/contribution of more distant neighbors to
            // the ith frequency
            //
            // for example the value at j = i has weight of 1 (full
            // contribution) when j = start or j = end the weight is 0
            float weight = 1.0f - (float)abs(i - j) / avg_size;

            sum += (*fft)[j] * weight;
            weight_sum += weight;
        }

        tmp[i] = sum / weight_sum;
    }

    memcpy(fft, tmp, sizeof(tmp));
}

void
apply_box_filter(float (*fft)[BUFFER_SIZE], int avg_size)
{
    float tmp[BUFFER_SIZE];

    for (int i = 0; i < BUFFER_SIZE; i++) {
        float sum = 0;

        int start = max(i - avg_size, 0);
        int end = min(i + avg_size, BUFFER_SIZE - 1);
        for (int j = start; j <= end; j++) {
            sum += (*fft)[j];
        }

        float avg = sum / ((end - start) + 1);

        tmp[i] = avg;
    }

    memcpy(fft, tmp, sizeof(tmp));
}

void
apply_block_avg(float (*fft)[BUFFER_SIZE], int avg_size)
{
    for (int i = 0; i < BUFFER_SIZE; i += avg_size) {
        float sum = 0;
        for (int j = i; j < min(i + avg_size, BUFFER_SIZE); j++) {
            sum += (*fft)[j];
        }

        float avg = sum / avg_size;

        for (int j = i; j < min(i + avg_size, BUFFER_SIZE); j++) {
            (*fft)[j] = avg;
        }
    }
}

void
avg_fft(auvi* a)
{
    switch (a->avg_mode) {
        case Block:
            apply_block_avg(&a->fft, a->avg_size);
            break;
        case BoxFilter:
            apply_box_filter(&a->fft, a->avg_size);
            break;
        case DoubleBoxFilter:
            apply_box_filter(&a->fft, a->avg_size);
            apply_box_filter(&a->fft, a->avg_size);
            break;
        case WeightedFilter:
            apply_weighted_avg(&a->fft, a->avg_size);
            break;
        case ExponentialFilter:
            apply_exponential_smoothing(&a->fft, a->alpha);
            break;
    }
}

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

    // scale down the whole result as we scale down the lower
    // frequencies more than the higher ones to fix spectral leakage a bit
    for (int i = 0; i < BUFFER_SIZE; i++) {
        fft_tmp[i] *= 0.04f + (0.5f * (i / (float)BUFFER_SIZE));
    }

    // iterating over N/2 because rfft returns only the positive half
    for (int i = 0; i < BUFFER_SIZE / 2; i += 2) {
        complex c = (complex){ fft_tmp[i], fft_tmp[i + 1] };
        float mag = cmp_abs(c);

        // remove noise from low mags
        mag = (0.7f * log10(1.1f * mag)) + (0.7f * mag);

        // clamp the mag between 0 and 1
        mag = clamp(mag, 0.0f, 1.0f);

        float prevmag = a->fft[i * 2];

        // update if mag is greater than the prev mag
        // we are leaving decline to the decay/fade out effect
        if (mag > prevmag) {
            a->fft[i * 2] = mag;
        }

        // fade out declining magnitudes
        if (mag < prevmag) {
            a->fft[i * 2] = prevmag * (((float)a->decay) / 100.0f);
        }

        // as the result fft from rfft is N/2
        // and half of the result again is imaginary numbers
        // we make bins of 4 values that are equal
        a->fft[(i * 2) + 1] = a->fft[i * 2];
        a->fft[(i * 2) + 2] = a->fft[i * 2];
        a->fft[(i * 2) + 3] = a->fft[i * 2];
    }

    // apply an averaging filter
    avg_fft(a);
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
    a.avg_mode = DoubleBoxFilter;
    a.avg_size = 8;
    a.alpha = 0.2;
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
