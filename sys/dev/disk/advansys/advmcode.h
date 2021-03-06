/*
 * Exported interface to downloadable microcode for AdvanSys SCSI Adapters
 *
 * $FreeBSD: src/sys/dev/advansys/advmcode.h,v 1.6 2000/01/14 03:33:38 gibbs Exp $
 * $DragonFly: src/sys/dev/disk/advansys/advmcode.h,v 1.2 2003/06/17 04:28:21 dillon Exp $
 *
 * Obtained from:
 *
 * Copyright (c) 1995-1999 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

extern u_int16_t adv_mcode[];
extern u_int16_t adv_mcode_size;
extern u_int32_t adv_mcode_chksum;
