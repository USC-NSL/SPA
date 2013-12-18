/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_graphics_3d_trusted.idl,
 *   modified Mon Nov 26 15:49:46 2012.
 */

#ifndef PPAPI_C_TRUSTED_PPB_GRAPHICS_3D_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_GRAPHICS_3D_TRUSTED_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_GRAPHICS_3D_TRUSTED_INTERFACE_1_0 "PPB_Graphics3DTrusted;1.0"
#define PPB_GRAPHICS_3D_TRUSTED_INTERFACE PPB_GRAPHICS_3D_TRUSTED_INTERFACE_1_0

/**
 * @file
 * Graphics 3D trusted interfaces. */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PPB_GRAPHICS3D_TRUSTED_ERROR_OK,
  PPB_GRAPHICS3D_TRUSTED_ERROR_GENERICERROR,
  PPB_GRAPHICS3D_TRUSTED_ERROR_INVALIDARGUMENTS,
  PPB_GRAPHICS3D_TRUSTED_ERROR_INVALIDSIZE,
  PPB_GRAPHICS3D_TRUSTED_ERROR_LOSTCONTEXT,
  PPB_GRAPHICS3D_TRUSTED_ERROR_OUTOFBOUNDS,
  PPB_GRAPHICS3D_TRUSTED_ERROR_UNKNOWNCOMMAND
} PPB_Graphics3DTrustedError;
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_Graphics3DTrustedState {
  /* Size of the command buffer in command buffer entries. */
  int32_t num_entries;
  /* The offset (in entries) from which the reader is reading. */
  int32_t get_offset;
  /* The offset (in entries) at which the writer is writing. */
  int32_t put_offset;
  /* The current token value. This is used by the writer to defer
   * changes to shared memory objects until the reader has reached a certain
   * point in the command buffer. The reader is responsible for updating the
   * token value, for example in response to an asynchronous set token command
   * embedded in the command buffer. The default token value is zero. */
  int32_t token;
  /* Error status. */
  PPB_Graphics3DTrustedError error;
  /* Generation index of this state. The generation index is incremented every
   * time a new state is retrieved from the command processor, so that
   * consistency can be kept even if IPC messages are processed out-of-order. */
  uint32_t generation;
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Graphics3DTrusted_1_0 {
  /* Creates a raw Graphics3D resource. A raw Graphics3D is intended to be used
   * with the trusted interface, through the command buffer (for proxying). */
  PP_Resource (*CreateRaw)(PP_Instance instance_id,
                           PP_Resource share_context,
                           const int32_t attrib_list[]);
  /* Initializes the command buffer with the given size. */
  PP_Bool (*InitCommandBuffer)(PP_Resource context_id);
  /* Sets the buffer used for commands. */
  PP_Bool (*SetGetBuffer)(PP_Resource context, int32_t transfer_buffer_id);
  /* Returns the current state. */
  struct PP_Graphics3DTrustedState (*GetState)(PP_Resource context);
  /* Create a transfer buffer and return a handle that uniquely
   * identifies it or -1 on error. */
  int32_t (*CreateTransferBuffer)(PP_Resource context, uint32_t size);
  /* Destroy a transfer buffer and recycle the handle. */
  PP_Bool (*DestroyTransferBuffer)(PP_Resource context, int32_t id);
  /* Get the transfer buffer associated with a handle. */
  PP_Bool (*GetTransferBuffer)(PP_Resource context,
                               int32_t id,
                               int* shm_handle,
                               uint32_t* shm_size);
  /* The writer calls this to update its put offset. */
  PP_Bool (*Flush)(PP_Resource context, int32_t put_offset);
  /* The writer calls this to update its put offset. This function returns the
   * reader's most recent get offset. Does not return until after the put offset
   * change callback has been invoked.
   * Note: This function remains for backwards compatibility; FlushSyncFast
   * is now the preferred way to sync. */
  struct PP_Graphics3DTrustedState (*FlushSync)(PP_Resource context,
                                                int32_t put_offset);
  /* Like FlushSync, but returns before processing commands if the get offset is
   * different than last_known_get. Allows synchronization with the command
   * processor without forcing immediate command execution. */
  struct PP_Graphics3DTrustedState (*FlushSyncFast)(PP_Resource context,
                                                    int32_t put_offset,
                                                    int32_t last_known_get);
};

typedef struct PPB_Graphics3DTrusted_1_0 PPB_Graphics3DTrusted;
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_GRAPHICS_3D_TRUSTED_H_ */

