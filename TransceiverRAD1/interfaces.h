/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2014 Range Networks, Inc.
 * 
 * This software is distributed under the terms of the GNU General Public 
 * License version 3. See the COPYING and NOTICE files in the current
 * directory for licensing information.
 * 
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

/*
 * We've now split the RAD1 into 3 separate interfaces.
 *
 * Interface 0 contains only ep0 and is used for command and status.
 * Interface 1 is the Tx path and it uses ep2 OUT BULK.
 * Interface 2 is the Rx path and it uses ep6 IN BULK.
 */

#define RAD1_CMD_INTERFACE              0
#define RAD1_CMD_ALTINTERFACE           0
#define RAD1_CMD_ENDPOINT               0

#define RAD1_TX_INTERFACE               1
#define RAD1_TX_ALTINTERFACE            0
#define RAD1_TX_ENDPOINT                2       // streaming data from host to FPGA

#define RAD1_RX_INTERFACE               2
#define RAD1_RX_ALTINTERFACE            0
#define RAD1_RX_ENDPOINT                6       // streaming data from FPGA to host

