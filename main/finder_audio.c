#include "finder_audio.h"

#include <stdbool.h>
#include <stddef.h>

#include "bsp_audio.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define FINDER_AUDIO_QUEUE_LENGTH 8U
#define FINDER_AUDIO_CHUNK 512U
#define FINDER_AUDIO_STACK 4096U
#define FINDER_AUDIO_PRIORITY 4U
/* Default kept deliberately modest: the beeps play repeatedly while walking,
 * so 70% was uncomfortably loud on first boot. */
#define FINDER_AUDIO_DEFAULT_VOLUME 40U

typedef enum {
    AUDIO_COMMAND_CUE = 0,
    AUDIO_COMMAND_VOLUME,
} audio_command_type_t;

typedef struct {
    audio_command_type_t type;
    finder_cue_t cue;
    finder_level_t level;
    uint32_t seed;
    uint8_t volume;
} finder_audio_command_t;

static QueueHandle_t s_queue;
static TaskHandle_t s_task;

static void audio_task(void *argument)
{
    finder_audio_synth_t synth;
    finder_audio_command_t command;
    int16_t samples[FINDER_AUDIO_CHUNK];
    uint8_t volume = FINDER_AUDIO_DEFAULT_VOLUME;
    bool playing = false;

    (void)argument;
    while (true) {
        if (xQueueReceive(s_queue, &command,
                          playing ? 0U : portMAX_DELAY) != pdTRUE) {
            if (!playing) {
                continue;
            }
        } else if (command.type == AUDIO_COMMAND_VOLUME) {
            volume = command.volume;
            bsp_audio_set_volume(volume);
            continue;
        } else if (!playing) {
            finder_audio_synth_begin(&synth, command.cue, command.level,
                                     command.seed);
            playing = true;
        }
        if (playing) {
            size_t produced = finder_audio_synth_render(&synth, samples,
                                                        FINDER_AUDIO_CHUNK);
            if (produced > 0U) {
                (void)bsp_audio_write(samples, produced * sizeof(samples[0]));
            }
            if (finder_audio_synth_done(&synth)) {
                playing = false;
            }
            continue;
        }
    }
}

esp_err_t finder_audio_init(void)
{
    esp_err_t err = bsp_audio_init();

    if (err != ESP_OK) {
        return err;
    }
    err = bsp_audio_set_format(FINDER_AUDIO_RATE, 16U, 1U);
    if (err != ESP_OK) {
        return err;
    }
    bsp_audio_set_volume(FINDER_AUDIO_DEFAULT_VOLUME);
    s_queue = xQueueCreate(FINDER_AUDIO_QUEUE_LENGTH, sizeof(finder_audio_command_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(audio_task, "finder_audio", FINDER_AUDIO_STACK, NULL,
                    FINDER_AUDIO_PRIORITY, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t finder_audio_play(finder_cue_t cue, finder_level_t level)
{
    finder_audio_command_t command;

    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    command.type = AUDIO_COMMAND_CUE;
    command.cue = cue;
    command.level = level;
    command.seed = esp_random();
    command.volume = 0U;
    if (xQueueSend(s_queue, &command, 0U) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t finder_audio_set_volume(uint8_t volume)
{
    finder_audio_command_t command;

    if (volume > 100U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    command.type = AUDIO_COMMAND_VOLUME;
    command.cue = FINDER_CUE_LEVEL;
    command.level = FINDER_LEVEL_OUT;
    command.seed = 0U;
    command.volume = volume;
    if (xQueueSend(s_queue, &command, 0U) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
