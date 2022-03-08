# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Realtek Semiconductor Corp.

sources += files(
        'rtk_ethdev.c',
        'rtk_rxtx.c',
)

error_cflags = [
        '-Wno-unused-parameter',
        '-Wno-unused-value',
        '-Wno-strict-aliasing',
        '-Wno-format-extra-args',
]
foreach flag: error_cflags
        if cc.has_argument(flag)
                cflags += flag
        endif
endforeach
