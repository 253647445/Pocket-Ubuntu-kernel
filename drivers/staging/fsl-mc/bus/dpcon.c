/* Copyright 2013-2016 Freescale Semiconductor Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "../include/mc-sys.h"
#include "../include/mc-cmd.h"
#include "../include/dpcon.h"

#include "dpcon-cmd.h"

/**
 * dpcon_open() - Open a control session for the specified object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @dpcon_id:	DPCON unique ID
 * @token:	Returned token; use in subsequent API calls
 *
 * This function can be used to open a control session for an
 * already created object; an object may have been declared in
 * the DPL or by calling the dpcon_create() function.
 * This function returns a unique authentication token,
 * associated with the specific object ID and the specific MC
 * portal; this token must be used in all subsequent commands for
 * this specific object.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpcon_open(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       int dpcon_id,
	       u16 *token)
{
	struct mc_command cmd = { 0 };
	struct dpcon_cmd_open *dpcon_cmd;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_OPEN,
					  cmd_flags,
					  0);
	dpcon_cmd = (struct dpcon_cmd_open *)cmd.params;
	dpcon_cmd->dpcon_id = cpu_to_le32(dpcon_id);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}
EXPORT_SYMBOL(dpcon_open);

/**
 * dpcon_close() - Close the control session of the object
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 *
 * After this function is called, no further operations are
 * allowed on the object without opening a new control session.
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpcon_close(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_CLOSE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL(dpcon_close);

/**
 * dpcon_enable() - Enable the DPCON
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpcon_enable(struct fsl_mc_io *mc_io,
		 u32 cmd_flags,
		 u16 token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_ENABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL(dpcon_enable);

/**
 * dpcon_disable() - Disable the DPCON
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpcon_disable(struct fsl_mc_io *mc_io,
		  u32 cmd_flags,
		  u16 token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_DISABLE,
					  cmd_flags,
					  token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL(dpcon_disable);

/**
 * dpcon_is_enabled() -	Check if the DPCON is enabled.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 * @en:		Returns '1' if object is enabled; '0' otherwise
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpcon_is_enabled(struct fsl_mc_io *mc_io,
		     u32 cmd_flags,
		     u16 token,
		     int *en)
{
	struct mc_command cmd = { 0 };
	struct dpcon_rsp_is_enabled *dpcon_rsp;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_IS_ENABLED,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	dpcon_rsp = (struct dpcon_rsp_is_enabled *)cmd.params;
	*en = dpcon_rsp->enabled & DPCON_ENABLE;

	return 0;
}
EXPORT_SYMBOL(dpcon_is_enabled);

/**
 * dpcon_reset() - Reset the DPCON, returns the object to initial state.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpcon_reset(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token)
{
	struct mc_command cmd = { 0 };

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_RESET,
					  cmd_flags, token);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL(dpcon_reset);

/**
 * dpcon_get_attributes() - Retrieve DPCON attributes.
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 * @attr:	Object's attributes
 *
 * Return:	'0' on Success; Error code otherwise.
 */
int dpcon_get_attributes(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 struct dpcon_attr *attr)
{
	struct mc_command cmd = { 0 };
	struct dpcon_rsp_get_attr *dpcon_rsp;
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	/* send command to mc*/
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	dpcon_rsp = (struct dpcon_rsp_get_attr *)cmd.params;
	attr->id = le32_to_cpu(dpcon_rsp->id);
	attr->qbman_ch_id = le16_to_cpu(dpcon_rsp->qbman_ch_id);
	attr->num_priorities = dpcon_rsp->num_priorities;

	return 0;
}
EXPORT_SYMBOL(dpcon_get_attributes);

/**
 * dpcon_set_notification() - Set DPCON notification destination
 * @mc_io:	Pointer to MC portal's I/O object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @token:	Token of DPCON object
 * @cfg:	Notification parameters
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpcon_set_notification(struct fsl_mc_io *mc_io,
			   u32 cmd_flags,
			   u16 token,
			   struct dpcon_notification_cfg *cfg)
{
	struct mc_command cmd = { 0 };
	struct dpcon_cmd_set_notification *dpcon_cmd;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_SET_NOTIFICATION,
					  cmd_flags,
					  token);
	dpcon_cmd = (struct dpcon_cmd_set_notification *)cmd.params;
	dpcon_cmd->dpio_id = cpu_to_le32(cfg->dpio_id);
	dpcon_cmd->priority = cfg->priority;
	dpcon_cmd->user_ctx = cpu_to_le64(cfg->user_ctx);

	/* send command to mc*/
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL(dpcon_set_notification);

/**
 * dpcon_get_api_version - Get Data Path Concentrator API version
 * @mc_io:	Pointer to MC portal's DPCON object
 * @cmd_flags:	Command flags; one or more of 'MC_CMD_FLAG_'
 * @major_ver:	Major version of DPCON API
 * @minor_ver:	Minor version of DPCON API
 *
 * Return:	'0' on Success; Error code otherwise
 */
int dpcon_get_api_version(struct fsl_mc_io *mc_io,
			  u32 cmd_flags,
			  u16 *major_ver,
			  u16 *minor_ver)
{
	struct mc_command cmd = { 0 };
	int err;

	/* prepare command */
	cmd.header = mc_encode_cmd_header(DPCON_CMDID_GET_API_VERSION,
					  cmd_flags, 0);

	/* send command to mc */
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	/* retrieve response parameters */
	mc_cmd_read_api_version(&cmd, major_ver, minor_ver);

	return 0;
}
EXPORT_SYMBOL(dpcon_get_api_version);
