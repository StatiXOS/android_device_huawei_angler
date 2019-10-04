/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 The LineageOS Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_NIDEBUG 0

#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define LOG_TAG "QCOM PowerHAL"
#include <log/log.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"

static int current_power_profile = PROFILE_BALANCED;

static int profile_high_performance[] = {
    SCHED_BOOST_ON, CPUS_ONLINE_MAX,
    ALL_CPUS_PWR_CLPS_DIS, 0x0901,
    CPU0_MIN_FREQ_TURBO_MAX,
    CPU1_MIN_FREQ_TURBO_MAX,
    CPU2_MIN_FREQ_TURBO_MAX,
    CPU3_MIN_FREQ_TURBO_MAX,
    CPU4_MIN_FREQ_TURBO_MAX,
    CPU5_MIN_FREQ_TURBO_MAX,
    CPU6_MIN_FREQ_TURBO_MAX,
    CPU7_MIN_FREQ_TURBO_MAX
};

static int profile_power_save[] = {
    CPUS_ONLINE_MPD_OVERRIDE, 0x0A03,
    CPU0_MAX_FREQ_NONTURBO_MAX - 2,
    CPU1_MAX_FREQ_NONTURBO_MAX - 2,
    CPU2_MAX_FREQ_NONTURBO_MAX - 2,
    CPU3_MAX_FREQ_NONTURBO_MAX - 2,
    CPU4_MAX_FREQ_NONTURBO_MAX - 2,
    CPU5_MAX_FREQ_NONTURBO_MAX - 2,
    CPU6_MAX_FREQ_NONTURBO_MAX - 2,
    CPU7_MAX_FREQ_NONTURBO_MAX - 2
};

static int profile_bias_power[] = {
    0x0A03, 0x0902,
    CPU0_MAX_FREQ_NONTURBO_MAX - 2,
    CPU1_MAX_FREQ_NONTURBO_MAX - 2,
    CPU2_MAX_FREQ_NONTURBO_MAX - 2,
    CPU3_MAX_FREQ_NONTURBO_MAX - 2,
    CPU4_MAX_FREQ_NONTURBO_MAX,
    CPU5_MAX_FREQ_NONTURBO_MAX,
    CPU6_MAX_FREQ_NONTURBO_MAX,
    CPU7_MAX_FREQ_NONTURBO_MAX
};

static int profile_bias_performance[] = {
    CPUS_ONLINE_MAX_LIMIT_MAX,
    CPU4_MIN_FREQ_NONTURBO_MAX + 1,
    CPU5_MIN_FREQ_NONTURBO_MAX + 1,
    CPU6_MIN_FREQ_NONTURBO_MAX + 1,
    CPU7_MIN_FREQ_NONTURBO_MAX + 1
};

#ifdef INTERACTION_BOOST
int get_number_of_profiles()
{
    return 5;
}
#endif

static int set_power_profile(void *data)
{
    int profile = data ? *((int*)data) : 0;
    int ret = -EINVAL;
    const char *profile_name = NULL;

    if (profile == current_power_profile)
        return 0;

    ALOGV("%s: Profile=%d", __func__, profile);

    if (current_power_profile != PROFILE_BALANCED) {
        undo_hint_action(DEFAULT_PROFILE_HINT_ID);
        ALOGV("%s: Hint undone", __func__);
        current_power_profile = PROFILE_BALANCED;
    }

    if (profile == PROFILE_POWER_SAVE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_power_save,
                ARRAY_SIZE(profile_power_save));
        profile_name = "powersave";

    } else if (profile == PROFILE_HIGH_PERFORMANCE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID,
                profile_high_performance, ARRAY_SIZE(profile_high_performance));
        profile_name = "performance";

    } else if (profile == PROFILE_BIAS_POWER) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_bias_power,
                ARRAY_SIZE(profile_bias_power));
        profile_name = "bias power";

    } else if (profile == PROFILE_BIAS_PERFORMANCE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID,
                profile_bias_performance, ARRAY_SIZE(profile_bias_performance));
        profile_name = "bias perf";
    } else if (profile == PROFILE_BALANCED) {
        ret = 0;
        profile_name = "balanced";
    }

    if (ret == 0) {
        current_power_profile = profile;
        ALOGD("%s: Set %s mode", __func__, profile_name);
    }
    return ret;
}

static void process_video_encode_hint(void *metadata)
{
    char governor[80];
    struct video_encode_metadata_t video_encode_metadata;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");
        return;
    }

    if (!metadata) {
        return;
    }

    /* Initialize encode metadata struct fields. */
    memset(&video_encode_metadata, 0, sizeof(struct video_encode_metadata_t));
    video_encode_metadata.state = -1;
    video_encode_metadata.hint_id = DEFAULT_VIDEO_ENCODE_HINT_ID;

    if (parse_video_encode_metadata((char *)metadata,
            &video_encode_metadata) == -1) {
        ALOGE("Error occurred while parsing metadata.");
        return;
    }

    if (video_encode_metadata.state == 1) {
        if (is_interactive_governor(governor)) {
            /* sched and cpufreq params
             * hispeed freq - 768 MHz
             * target load - 90
             * above_hispeed_delay - 40ms
             * sched_small_tsk - 50
             */
            int resource_values[] = {
                0x2C07, 0x2F5A, 0x2704, 0x4032
            };
            perform_hint_action(video_encode_metadata.hint_id,
                    resource_values, ARRAY_SIZE(resource_values));
        }
    } else if (video_encode_metadata.state == 0) {
        if (is_interactive_governor(governor)) {
            undo_hint_action(video_encode_metadata.hint_id);
        }
    }
}

static int resources_interaction_fling_boost[] = {
    ALL_CPUS_PWR_CLPS_DIS,
    SCHED_BOOST_ON,
    SCHED_PREFER_IDLE_DIS
};

static int resources_interaction_boost[] = {
    ALL_CPUS_PWR_CLPS_DIS,
    SCHED_PREFER_IDLE_DIS
};

static int resources_launch[] = {
    SCHED_BOOST_ON,
    0x20C
};

static int process_activity_launch_hint(void *data)
{
    static int launch_handle = -1;
    static int launch_mode = 0;

    // release lock early if launch has finished
    if (!data) {
        if (CHECK_HANDLE(launch_handle)) {
            release_request(launch_handle);
            launch_handle = -1;
        }
        launch_mode = 0;
        return HINT_HANDLED;
    }

    if (!launch_mode) {
        launch_handle = interaction_with_handle(launch_handle, 5000,
                ARRAY_SIZE(resources_launch), resources_launch);
        if (!CHECK_HANDLE(launch_handle)) {
            ALOGE("Failed to perform launch boost");
            return HINT_NONE;
        }
        launch_mode = 1;
    }
    return HINT_HANDLED;
}

int power_hint_override(power_hint_t hint, void *data)
{
    static struct timespec s_previous_boost_timespec;
    struct timespec cur_boost_timespec;
    long long elapsed_time;
    static int s_previous_duration = 0;
    int duration;

    if (hint == POWER_HINT_SET_PROFILE) {
        if (set_power_profile(data) < 0)
            ALOGE("Setting power profile failed. perfd not started?");
        return HINT_HANDLED;
    }

    // Skip other hints in high/low power modes
    if (current_power_profile == PROFILE_POWER_SAVE ||
            current_power_profile == PROFILE_HIGH_PERFORMANCE) {
        return HINT_HANDLED;
    }

    switch (hint) {
        case POWER_HINT_INTERACTION:
            duration = 500; // 500ms by default
            if (data) {
                int input_duration = *((int*)data);
                if (input_duration > duration) {
                    duration = (input_duration > 5000) ? 5000 : input_duration;
                }
            }

            clock_gettime(CLOCK_MONOTONIC, &cur_boost_timespec);

            elapsed_time = calc_timespan_us(s_previous_boost_timespec, cur_boost_timespec);
            // don't hint if previous hint's duration covers this hint's duration
            if ((s_previous_duration * 1000) > (elapsed_time + duration * 1000)) {
                return HINT_HANDLED;
            }
            s_previous_boost_timespec = cur_boost_timespec;
            s_previous_duration = duration;

            if (duration >= 1500) {
                interaction(duration, ARRAY_SIZE(resources_interaction_fling_boost),
                        resources_interaction_fling_boost);
            } else {
                interaction(duration, ARRAY_SIZE(resources_interaction_boost),
                        resources_interaction_boost);
            }
            return HINT_HANDLED;
        case POWER_HINT_LAUNCH:
            return process_activity_launch_hint(data);
        case POWER_HINT_VIDEO_ENCODE:
            process_video_encode_hint(data);
            return HINT_HANDLED;
        default:
            break;
    }
    return HINT_NONE;
}

int set_interactive_override(int on)
{
    char governor[80];

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");
        return HINT_NONE;
    }

    if (!on) {
        /* Display off */
        if (is_interactive_governor(governor)) {
            // sched upmigrate = 99, sched downmigrate = 95
            // keep the big cores around, but make them very hard to use
            int resource_values[] = {
                0x4E63, 0x4F5F
            };
            perform_hint_action(DISPLAY_STATE_HINT_ID,
                    resource_values, ARRAY_SIZE(resource_values));
        }
    } else {
        /* Display on */
        if (is_interactive_governor(governor)) {
            undo_hint_action(DISPLAY_STATE_HINT_ID);
        }
    }
    return HINT_HANDLED;
}
