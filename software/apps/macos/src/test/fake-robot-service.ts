import type {
  AxisIdentifier,
  AxisState,
  RawRegisterEntry,
  RobotWorkspaceSnapshot,
} from "../models/robot";
import type {
  AxisRegisterSnapshot,
  JogDirection,
  RobotService,
} from "../services/robot-service";

type InternalAxisState = AxisState;

const axisNames = [
  "Base",
  "Shoulder",
  "Elbow",
  "Wrist pitch",
  "Wrist roll",
  "Tool",
] as const;

function axisId(index: number) {
  return `axis-${index + 1}` as AxisIdentifier;
}

function makeAxis(index: number): InternalAxisState {
  return {
    id: axisId(index),
    index: index + 1,
    shortLabel: `A${index + 1}`,
    name: axisNames[index]!,
    connected: true,
    devicePath: `/dev/cu.test-axis-${index + 1}`,
    baudRate: 115200,
    controllerAxisId: index + 1,
    status: "armed",
    enabled: true,
    fault: null,
    positionDeg: index * 4.5,
    targetPositionDeg: index * 4.5,
    velocityDegPerSec: index === 0 ? 0.4 : 0,
    currentAmps: 0,
    busVoltage: 0,
    temperatureC: 0,
    controlMode: "Position",
    lastCommand: "Axis ready for direct manipulation",
  };
}

function registerSet(axis: InternalAxisState): RawRegisterEntry[] {
  return [
    {
      address: 0x0002,
      name: "AXIS_ID",
      value: axis.index,
      rawValue: `0x${axis.index.toString(16).padStart(8, "0").toUpperCase()}`,
      writable: true,
      changed: false,
    },
    {
      address: 0x0010,
      name: "CONTROL_WORD",
      value: axis.enabled ? 0x0000_0003 : 0,
      rawValue: axis.enabled ? "0x00000003" : "0x00000000",
      writable: true,
      changed: false,
    },
    {
      address: 0x0011,
      name: "STATUS_WORD",
      value: axis.connected ? 0x0000_0003 : 0,
      rawValue: axis.connected ? "0x00000003" : "0x00000000",
      writable: false,
      changed: false,
    },
    {
      address: 0x0012,
      name: "FAULT_WORD",
      value: axis.fault ? 0x0000_0001 : 0,
      rawValue: axis.fault ? "0x00000001" : "0x00000000",
      writable: false,
      changed: Boolean(axis.fault),
    },
    {
      address: 0x0022,
      name: "CURRENT_POSITION_STEPS",
      value: Math.round(axis.positionDeg * 100),
      rawValue: `0x${Math.round(axis.positionDeg * 100)
        .toString(16)
        .padStart(8, "0")
        .toUpperCase()}`,
      writable: false,
      changed: true,
    },
    {
      address: 0x0025,
      name: "CURRENT_VELOCITY_STEPS_S",
      value: Math.round(axis.velocityDegPerSec * 100),
      rawValue: `0x${Math.round(axis.velocityDegPerSec * 100)
        .toString(16)
        .padStart(8, "0")
        .toUpperCase()}`,
      writable: false,
      changed: true,
    },
    {
      address: 0x0045,
      name: "SOFT_LIMIT_MIN_STEPS",
      value: 0xffff_f830,
      rawValue: "0xFFFFF830",
      writable: true,
      changed: false,
    },
    {
      address: 0x0046,
      name: "SOFT_LIMIT_MAX_STEPS",
      value: 0x0000_07d0,
      rawValue: "0x000007D0",
      writable: true,
      changed: false,
    },
  ];
}

function cloneSnapshot(snapshot: RobotWorkspaceSnapshot): RobotWorkspaceSnapshot {
  return {
    ...snapshot,
    connection: { ...snapshot.connection },
    axes: snapshot.axes.map((axis) => ({ ...axis })),
    diagnostics: snapshot.diagnostics.map((event) => ({ ...event })),
    availablePorts: [...snapshot.availablePorts],
  };
}

export function createFakeRobotService(): RobotService {
  let axes = Array.from({ length: 6 }, (_, index) => makeAxis(index));

  const snapshot = (): RobotWorkspaceSnapshot => ({
    timestamp: Date.now(),
    connection: {
      mode: "live",
      label: "Live (6)",
      detail: "Six fake axis controllers connected for component tests.",
      latencyMs: 12,
      pollHz: 4,
    },
    modeLabel: "Live serial console",
    axes: axes.map((axis) => ({ ...axis })),
    diagnostics: [
      {
        id: "diag-1",
        level: "info",
        source: "Workspace",
        message: "Test backend ready",
        timestampLabel: "09:00:00",
      },
    ],
    availablePorts: axes.map((axis) => axis.devicePath ?? ""),
  });

  return {
    async getWorkspaceSnapshot() {
      return cloneSnapshot(snapshot());
    },
    async connectAxis(axisId, devicePath, baudRate) {
      axes = axes.map((axis) =>
        axis.id === axisId
          ? {
              ...axis,
              connected: true,
              devicePath,
              baudRate,
              lastCommand: "Controller connected",
              status: "armed",
            }
          : axis,
      );
      return cloneSnapshot(snapshot());
    },
    async disconnectAxis(axisId) {
      axes = axes.map((axis) =>
        axis.id === axisId
          ? {
              ...axis,
              connected: false,
              enabled: false,
              status: "disconnected",
              lastCommand: "Controller disconnected",
            }
          : axis,
      );
      return cloneSnapshot(snapshot());
    },
    async jogAxis(axisId, direction, stepDeg) {
      const delta = direction === "negative" ? -stepDeg : stepDeg;
      axes = axes.map((axis) =>
        axis.id === axisId
          ? {
              ...axis,
              positionDeg: Number((axis.positionDeg + delta).toFixed(2)),
              targetPositionDeg: Number((axis.positionDeg + delta).toFixed(2)),
              velocityDegPerSec: delta,
              status: "jogging",
              lastCommand: `Step jog ${delta > 0 ? "+" : ""}${delta.toFixed(1)} deg`,
            }
          : axis,
      );
      return cloneSnapshot(snapshot());
    },
    async clearAxisFault(axisId) {
      axes = axes.map((axis) =>
        axis.id === axisId
          ? {
              ...axis,
              fault: null,
              status: "armed",
              lastCommand: "Clear fault requested",
            }
          : axis,
      );
      return cloneSnapshot(snapshot());
    },
    async setAxisZero(axisId) {
      axes = axes.map((axis) =>
        axis.id === axisId
          ? {
              ...axis,
              positionDeg: 0,
              targetPositionDeg: 0,
              lastCommand: "Set zero requested",
            }
          : axis,
      );
      return cloneSnapshot(snapshot());
    },
    async getAxisRegisters(axisId): Promise<AxisRegisterSnapshot> {
      const axis = axes.find((entry) => entry.id === axisId) ?? axes[0]!;
      return {
        axisId,
        entries: registerSet(axis),
        error: null,
      };
    },
  };
}
