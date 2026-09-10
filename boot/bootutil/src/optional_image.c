/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Copyright (c) 2026 Nordic Semiconductor ASA
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bootutil/bootutil_log.h"
#include "bootutil_area.h"
#include "bootutil_priv.h"

BOOT_LOG_MODULE_DECLARE(mcuboot);

/**
 * Check whether a location in a flash area still holds data.
 *
 * @param fap Flash area to inspect.
 * @param off Offset within the flash area.
 *
 * @return true if the location is not in the erased state.
 */
static bool boot_region_has_data(const struct flash_area *fap, uint32_t off)
{
    uint32_t erased_word;
    uint32_t first_word;

    if (flash_area_read(fap, off, &first_word, sizeof(first_word)) != 0) {
        /* The state cannot be determined, so assume a scrub is still needed. */
        return true;
    }

    memset(&erased_word, flash_area_erased_val(fap), sizeof(erased_word));

    return first_word != erased_word;
}

/**
 * Scrub the primary slot of an optional image that failed validation.
 *
 * Only the first sector is scrubbed. Dropping the image header is what makes
 * the application treat the partition as empty, and limiting the write to a
 * single sector keeps the boot short for slots that may be large.
 *
 * The outcome is reported here rather than by the caller, so that a boot that
 * finds the slot already scrubbed is not reported as having scrubbed it again.
 *
 * @param state Bootloader state, with BOOT_CURR_IMG set to the image to scrub.
 */
void boot_scrub_optional_image(struct boot_loader_state *state)
{
    const struct flash_area *fap = BOOT_IMG_AREA(state, BOOT_SLOT_PRIMARY);
    uint32_t off;
    uint32_t size;
    int rc;

    if (fap == NULL) {
        BOOT_LOG_ERR("Failed to scrub image %d: no flash area", BOOT_CURR_IMG(state));
        return;
    }

    if (boot_img_num_sectors(state, BOOT_SLOT_PRIMARY) == 0) {
        /* The sector layout is unknown, so the header cannot be mapped onto a
         * sector; fall back to scrubbing the whole slot.
         */
        off = 0;
        size = flash_area_get_size(fap);
    } else {
        off = boot_img_sector_off(state, BOOT_SLOT_PRIMARY, 0);
        size = boot_img_sector_size(state, BOOT_SLOT_PRIMARY, 0);
    }

    if (!boot_region_has_data(fap, off)) {
        /* Already scrubbed on an earlier boot; skip the write so that the
         * memory is not rewritten on every reset.
         */
        return;
    }

    rc = boot_scramble_region(fap, off, size, false);
    if (rc != 0) {
        BOOT_LOG_ERR("Failed to scrub image %d: %d", BOOT_CURR_IMG(state), rc);
    } else {
        BOOT_LOG_INF("Scrubbed image %d in the primary slot", BOOT_CURR_IMG(state));
    }
}
