#!/system/bin/sh

################################################################################
# helper functions to allow Android init like script

function write() {
    echo -n $2 > $1
}

function copy() {
    cat $1 > $2
}

function get-set-forall() {
    for f in $1 ; do
        cat $f
        write $f $2
    done
}

################################################################################

# disable thermal bcl hotplug to switch governor
write /sys/module/msm_thermal/core_control/enabled 0
get-set-forall /sys/devices/soc.0/qcom,bcl.*/mode disable
bcl_hotplug_mask=`get-set-forall /sys/devices/soc.0/qcom,bcl.*/hotplug_mask 0`
bcl_hotplug_soc_mask=`get-set-forall /sys/devices/soc.0/qcom,bcl.*/hotplug_soc_mask 0`
get-set-forall /sys/devices/soc.0/qcom,bcl.*/mode enable

# some files in /sys/devices/system/cpu are created after the restorecon of
# /sys/. These files receive the default label "sysfs".
# Restorecon again to give new files the correct label.
restorecon -R /sys/devices/system/cpu

# ensure at most one A57 is online when thermal hotplug is disabled
write /sys/devices/system/cpu/cpu5/online 0
write /sys/devices/system/cpu/cpu6/online 0
write /sys/devices/system/cpu/cpu7/online 0

# Best effort limiting for first time boot if msm_performance module is absent
write /sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq 960000

# Limit A57 max freq from msm_perf module in case CPU 4 is offline
write /sys/module/msm_performance/parameters/cpu_max_freq "4:960000 5:960000 6:960000 7:960000"

# configure governor settings for little cluster
write /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor interactive
restorecon -R /sys/devices/system/cpu # must restore after interactive
write /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq 384000
write /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 1555200
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/go_hispeed_load 93
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/above_hispeed_delay "0 600000:19000 672000:20000 960000:24000 1248000:38000"
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/timer_rate 50000
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/hispeed_freq 600000
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/timer_slack 380000
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/target_loads "29 384000:88 600000:90 672000:92 960000:93 1248000:98"
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/min_sample_time 60000
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/boost 0
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/align_windows 1
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/use_migration_notif 1
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/use_sched_load 0
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/max_freq_hysteresis 0
write /sys/devices/system/cpu/cpu0/cpufreq/interactive/boostpulse_duration 0

# online CPU4
write /sys/devices/system/cpu/cpu4/online 1

# configure governor settings for big cluster
write /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor interactive
restorecon -R /sys/devices/system/cpu # must restore after interactive
write /sys/devices/system/cpu/cpu4/cpufreq/scaling_min_freq 384000
write /sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq 1958400
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/go_hispeed_load 150
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/above_hispeed_delay "20000 960000:40000 1248000:30000"
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/timer_rate 60000
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/hispeed_freq 960000
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/target_loads 98
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/min_sample_time 60000
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/boost 0
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/align_windows 1
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/use_migration_notif 1
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/use_sched_load 0
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/max_freq_hysteresis 0
write /sys/devices/system/cpu/cpu4/cpufreq/interactive/boostpulse_duration 0

# restore A57's max
copy /sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_max_freq /sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq

# plugin remaining A57s
write /sys/devices/system/cpu/cpu5/online 1
write /sys/devices/system/cpu/cpu6/online 1
write /sys/devices/system/cpu/cpu7/online 1

# Restore CPU 4 max freq from msm_performance
write /sys/module/msm_performance/parameters/cpu_max_freq "4:4294967295 5:4294967295 6:4294967295 7:4294967295"

# input boost configuration
write /sys/module/cpu_boost/parameters/input_boost_enabled 1
write /sys/module/cpu_boost/parameters/boost_ms 40
write /sys/module/cpu_boost/parameters/input_boost_freq "0:600000 1:600000 2:600000 3:600000 4:960000 5:960000 6:960000 7:960000"
write /sys/module/cpu_boost/parameters/input_boost_ms 300
write /sys/module/cpu_boost/parameters/load_based_syncs Y
write /sys/module/cpu_boost/parameters/migration_load_threshold 15
write /sys/module/cpu_boost/parameters/sync_threshold 1248000

# Setting B.L scheduler parameters
write /proc/sys/kernel/sched_migration_fixup 1
write /proc/sys/kernel/sched_upmigrate 95
write /proc/sys/kernel/sched_downmigrate 85
write /proc/sys/kernel/sched_freq_inc_notify 400000
write /proc/sys/kernel/sched_freq_dec_notify 400000

# android background processes are set to nice 10. Never schedule these on the a57s.
write /proc/sys/kernel/sched_upmigrate_min_nice 9

get-set-forall  /sys/class/devfreq/qcom,cpubw*/governor bw_hwmon

# Disable sched_boost
write /proc/sys/kernel/sched_boost 0

# re-enable thermal and BCL hotplug
write /sys/module/msm_thermal/core_control/enabled 1
get-set-forall /sys/devices/soc.0/qcom,bcl.*/mode disable
get-set-forall /sys/devices/soc.0/qcom,bcl.*/hotplug_mask $bcl_hotplug_mask
get-set-forall /sys/devices/soc.0/qcom,bcl.*/hotplug_soc_mask $bcl_hotplug_soc_mask
get-set-forall /sys/devices/soc.0/qcom,bcl.*/mode enable

# change GPU initial power level from 305MHz(level 4) to 180MHz(level 5) for power savings
write /sys/class/kgsl/kgsl-3d0/default_pwrlevel 5
