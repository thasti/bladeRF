/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdlib.h>
#include "bladerf_priv.h"
#include "dc_cal_table.h"
#include "file_ops.h"
#include "log.h"

static inline void load_dc_cal(struct bladerf *dev, const char *file)
{
    int status;
    struct bladerf_image *img;

    img = bladerf_alloc_image(BLADERF_IMAGE_TYPE_INVALID, 0, 0);
    if (img == NULL) {
        return;
    }

    status = bladerf_image_read(img, file);
    if (status != 0) {
        log_debug("Failed to open image file (%s): %s\n",
                  file, bladerf_strerror(status));
        return;
    }

    switch (img->type) {
        case BLADERF_IMAGE_TYPE_RX_DC_CAL:
            dev->cal.dc_rx = dc_cal_tbl_load(img->data, img->length);
            break;

        case BLADERF_IMAGE_TYPE_TX_DC_CAL:
            dev->cal.dc_tx = dc_cal_tbl_load(img->data, img->length);
            break;

        default:
            log_debug("%s is not an RX DC calibration table.\n", file);
    }
}

static inline int load_fpga(struct bladerf *dev)
{
    int status;
    uint8_t *buf;
    size_t buf_len;

    if (dev->fpga_size == BLADERF_FPGA_40KLE) {
        status = file_find_and_read("hostedx40.rbf", &buf, &buf_len);
    } else if (dev->fpga_size == BLADERF_FPGA_115KLE) {
        status = file_find_and_read("hostedx115.rbf", &buf, &buf_len);
    } else {
        /* Unexpected, but this will be complained about elsewhere */
        return 0;
    }

    if (status == 0) {
        log_verbose("Loading FPGA config directory\n");
        return dev->fn->load_fpga(dev, buf, buf_len);
    } else if (status == BLADERF_ERR_NO_FILE) {
        return 0;
    } else {
        return status;
    }
}

static inline int load_dc_cals(struct bladerf *dev)
{
    char *filename;
    char *full_path;

    filename = calloc(1, FILENAME_MAX + 1);
    if (filename == NULL) {
        return BLADERF_ERR_MEM;
    }

    strncat(filename, dev->ident.serial, FILENAME_MAX);
    strncat(filename, "_dc_rx.bin", FILENAME_MAX - BLADERF_SERIAL_LENGTH);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_verbose("Loading %s\n", full_path);
        load_dc_cal(dev, full_path);
        free(full_path);
    }

    memset(filename, 0, FILENAME_MAX + 1);
    strncat(filename, dev->ident.serial, FILENAME_MAX);
    strncat(filename, "_dc_tx.bin", FILENAME_MAX - BLADERF_SERIAL_LENGTH);
    full_path = file_find(filename);
    if (full_path != NULL) {
        log_verbose("Loading %s\n", full_path);
        load_dc_cal(dev, full_path);
        free(full_path);
    }

    free(filename);
    return 0;
}

int config_load_all(struct bladerf *dev)
{
    int status;

    status = load_dc_cals(dev);
    if (status != 0) {
        return status;
    }

    status = dev->fn->is_fpga_configured(dev);
    if (status == 0) {
        status = load_fpga(dev);
    } else if (status > 0) {
        status = 0;
    }

    return status;
}
