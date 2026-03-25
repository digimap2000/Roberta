import { invoke } from "@tauri-apps/api/core";

import type {
  AxisIdentifier,
  RawRegisterEntry,
  RobotWorkspaceSnapshot,
} from "../models/robot";
import type {
  AxisRegisterSnapshot,
  JogDirection,
  RobotService,
} from "./robot-service";

type AxisCommandPayload = {
  axisId: AxisIdentifier;
};

type ConnectAxisPayload = AxisCommandPayload & {
  devicePath: string;
  baudRate: number;
};

type JogAxisPayload = AxisCommandPayload & {
  direction: JogDirection;
  stepDeg: number;
};

type BackendAxisRegisterSnapshot = {
  axisId: AxisIdentifier;
  entries: Array<{
    address: number;
    name: string;
    value: number;
    rawValue: string;
    writable: boolean;
    changed: boolean;
  }>;
  error: string | null;
};

function normalizeRegisterEntry(entry: BackendAxisRegisterSnapshot["entries"][number]): RawRegisterEntry {
  return {
    address: entry.address,
    name: entry.name,
    value: entry.value,
    rawValue: entry.rawValue,
    writable: entry.writable,
    changed: entry.changed,
  };
}

export function createLiveRobotService(): RobotService {
  return {
    async getWorkspaceSnapshot() {
      return invoke<RobotWorkspaceSnapshot>("get_workspace_snapshot");
    },
    async connectAxis(axisId, devicePath, baudRate) {
      return invoke<RobotWorkspaceSnapshot>("connect_axis", {
        payload: {
          axisId,
          devicePath,
          baudRate,
        } satisfies ConnectAxisPayload,
      });
    },
    async disconnectAxis(axisId) {
      return invoke<RobotWorkspaceSnapshot>("disconnect_axis", {
        payload: {
          axisId,
        } satisfies AxisCommandPayload,
      });
    },
    async jogAxis(axisId, direction, stepDeg) {
      return invoke<RobotWorkspaceSnapshot>("jog_axis", {
        payload: {
          axisId,
          direction,
          stepDeg,
        } satisfies JogAxisPayload,
      });
    },
    async clearAxisFault(axisId) {
      return invoke<RobotWorkspaceSnapshot>("clear_axis_fault", {
        payload: {
          axisId,
        } satisfies AxisCommandPayload,
      });
    },
    async setAxisZero(axisId) {
      return invoke<RobotWorkspaceSnapshot>("set_axis_zero", {
        payload: {
          axisId,
        } satisfies AxisCommandPayload,
      });
    },
    async getAxisRegisters(axisId) {
      const result = await invoke<BackendAxisRegisterSnapshot>("get_axis_registers", {
        payload: {
          axisId,
        } satisfies AxisCommandPayload,
      });

      return {
        axisId: result.axisId,
        entries: result.entries.map(normalizeRegisterEntry),
        error: result.error,
      } satisfies AxisRegisterSnapshot;
    },
  };
}
