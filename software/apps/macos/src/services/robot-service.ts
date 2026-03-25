import type {
  AxisIdentifier,
  RawRegisterEntry,
  RobotWorkspaceSnapshot,
} from "../models/robot";

export type JogDirection = "negative" | "positive";

export type AxisRegisterSnapshot = {
  axisId: AxisIdentifier;
  entries: RawRegisterEntry[];
  error: string | null;
};

export type RobotService = {
  getWorkspaceSnapshot: () => Promise<RobotWorkspaceSnapshot>;
  connectAxis: (
    axisId: AxisIdentifier,
    devicePath: string,
    baudRate: number,
  ) => Promise<RobotWorkspaceSnapshot>;
  disconnectAxis: (axisId: AxisIdentifier) => Promise<RobotWorkspaceSnapshot>;
  jogAxis: (
    axisId: AxisIdentifier,
    direction: JogDirection,
    stepDeg: number,
  ) => Promise<RobotWorkspaceSnapshot>;
  clearAxisFault: (axisId: AxisIdentifier) => Promise<RobotWorkspaceSnapshot>;
  setAxisZero: (axisId: AxisIdentifier) => Promise<RobotWorkspaceSnapshot>;
  getAxisRegisters: (axisId: AxisIdentifier) => Promise<AxisRegisterSnapshot>;
};
