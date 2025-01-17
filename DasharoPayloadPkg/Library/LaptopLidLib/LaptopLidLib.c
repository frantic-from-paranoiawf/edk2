/** @file
  Library that query laptop EC for lid state.

Copyright (c) 2024, 3mdeb. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/LaptopLidLib.h>

#define EC_POLL_DELAY_US        10
#define EC_SEND_TIMEOUT_US      20000	// 20ms
#define EC_RECV_TIMEOUT_US      320000	// 320ms

#define EC_SC                   0x66
#define EC_DATA                 0x62

#define EC_CMD                  (1 << 3)
#define EC_IBF                  (1 << 1)
#define EC_OBF                  (1 << 0)

#define RD_EC                   0x80

#define LID_STATE_REG           0x3
#define   LID_STATE             (1 << 0)


RETURN_STATUS
EcScWait (
  UINTN            TimeoutUs,
  UINT8            Mask,
  UINT8            State
  )
{
  while (TimeoutUs > 0 && (IoRead8(EC_SC) & Mask) != State) {
    MicroSecondDelay(EC_POLL_DELAY_US);
    TimeoutUs -= EC_POLL_DELAY_US;
  }

	return TimeoutUs > 0 ? RETURN_SUCCESS : RETURN_TIMEOUT;
}

RETURN_STATUS
EcReadySend (
  UINTN            TimeoutUs
  )
{
	return EcScWait(TimeoutUs, EC_IBF, 0);
}

RETURN_STATUS
EcReadyRecv (
  UINTN            TimeoutUs
  )
{
	return EcScWait(TimeoutUs, EC_OBF, EC_OBF);
}

RETURN_STATUS
EcRecvDataTimeout (
  UINT8            *Data,
  UINTN            TimeoutUs
  )
{
  EFI_STATUS       Status;

  if (!Data)
    return RETURN_INVALID_PARAMETER;

  Status = EcReadyRecv(TimeoutUs);

  if (Status != RETURN_SUCCESS)
      return Status;

  *Data = IoRead8(EC_DATA);

  return RETURN_SUCCESS;
}

RETURN_STATUS
EcSendDataTimeout (
  UINT8            Data,
  UINTN            TimeoutUs
  )
{
  EFI_STATUS       Status;

  Status = EcReadySend(TimeoutUs);

  if (Status != RETURN_SUCCESS)
      return Status;

  IoWrite8(EC_DATA, Data);

  return RETURN_SUCCESS;
}

RETURN_STATUS
EcSendCmdTimeout (
  UINT8            Cmd,
  UINTN            TimeoutUs
  )
{
  EFI_STATUS       Status;

  if (!Cmd)
    return RETURN_INVALID_PARAMETER;

  Status = EcReadySend(TimeoutUs);

  if (Status != RETURN_SUCCESS)
      return Status;

  IoWrite8(EC_SC, Cmd);

  return RETURN_SUCCESS;
}

RETURN_STATUS
EcSendCmd (
  UINT8            Cmd
  )
{
	return EcSendCmdTimeout(Cmd, EC_SEND_TIMEOUT_US);
}

RETURN_STATUS
EcSendData (
  UINT8            Data
  )
{
	return EcSendDataTimeout(Data, EC_SEND_TIMEOUT_US);
}

RETURN_STATUS
EcRecvData (
  UINT8            *Data
  )
{
	return EcRecvDataTimeout(Data, EC_RECV_TIMEOUT_US);
}

RETURN_STATUS
EcReadReg (
  UINT8           Reg,
  UINT8           *Data
  )
{
  EFI_STATUS   Status;

  Status = EcSendCmd(RD_EC);

  if (Status != RETURN_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Failed to send read EC command for reg %02x: %r\n", Reg, Status));
    return Status;
  }

  Status = EcSendData(Reg);

  if (Status != RETURN_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Failed to send read EC address %02x: %r\n", Reg, Status));
    return Status;
  }

	return EcRecvData(Data);
}

RETURN_STATUS
EcReadReg32 (
  UINT8           Reg,
  UINT32          *Data32
  )
{
  RETURN_STATUS   Status;
  UINT8           Data8[4];
  UINTN           Index;

  if (!Data32)
    return RETURN_INVALID_PARAMETER;

  *Data32 = 0;

  for (Index = 0; Index < 4; Index++) {
    Status = EcReadReg(Reg + Index, &Data8[Index]);

    if (Status != RETURN_SUCCESS)
        return Status;

    *Data32 |= (UINT32)Data8[Index] << (8 * Index);
  }

  return RETURN_SUCCESS;
}

/**
  This function retrieves the lid state from EC.

  @param  LidState                    Pointer to the lid state

  @retval RETURN_SUCCESS              Successfully probed the lid state.
  @retval RETURN_UNSUPPORTED          Function is unsupported.
  @retval RETURN_TIMEOUT              EC communication timeout.
  @retval RETURN_INVALID_PARAMETER    NULL pointer passed as parameter

**/
EFI_STATUS
EFIAPI
LaptopGetLidState (
  LID_STATUS           *LidState
  )
{
  EFI_STATUS        Status;
  UINT8             Reg;

  Status = EcReadReg(LID_STATE_REG, &Reg);

  if (Status != RETURN_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Failed to read lid state: %r\n", Status));
    return Status;
  }

  *LidState = (Reg & LID_STATE) ? LidOpen : LidClosed;

  DEBUG ((DEBUG_INFO, "Lid %a\n", *LidState ? "open" : "closed"));

  return RETURN_SUCCESS;
}

