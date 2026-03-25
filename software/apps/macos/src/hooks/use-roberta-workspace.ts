import { startTransition, useEffect, useMemo, useReducer, useState } from "react";

import type {
  AxisIdentifier,
  AxisState,
  DiagnosticsEvent,
  RegisterEntry,
  RegisterGroup,
  RobotWorkspaceSnapshot,
  WorkspaceSection,
} from "../models/robot";
import { createLiveRobotService } from "../services/live-robot-service";
import { buildRegisterGroups } from "../services/register-groups";
import type { JogDirection, RobotService } from "../services/robot-service";

type AxisConnectionDraft = {
  devicePath: string;
  baudRate: number;
};

type WorkspaceState = {
  activeSection: WorkspaceSection;
  selectedAxisId: AxisIdentifier;
  selectedRegisterGroupId: string | null;
  selectedRegisterKey: string | null;
  commandPaletteOpen: boolean;
  commandQuery: string;
  diagnosticsDockOpen: boolean;
  jogStepDeg: number;
  snapshot: RobotWorkspaceSnapshot | null;
  registerGroupsByAxis: Partial<Record<AxisIdentifier, RegisterGroup[]>>;
  registerErrorsByAxis: Partial<Record<AxisIdentifier, string | null>>;
  axisConnectionDrafts: Partial<Record<AxisIdentifier, AxisConnectionDraft>>;
};

type WorkspaceAction =
  | { type: "workspace/set-snapshot"; snapshot: RobotWorkspaceSnapshot }
  | {
      type: "workspace/set-axis-registers";
      axisId: AxisIdentifier;
      groups: RegisterGroup[];
      error: string | null;
    }
  | { type: "workspace/select-section"; section: WorkspaceSection }
  | { type: "workspace/select-axis"; axisId: AxisIdentifier }
  | { type: "workspace/select-register"; groupId: string; registerKey: string }
  | { type: "workspace/set-command-palette"; open: boolean }
  | { type: "workspace/set-command-query"; query: string }
  | { type: "workspace/toggle-diagnostics-dock" }
  | { type: "workspace/set-jog-step"; stepDeg: number }
  | {
      type: "workspace/update-axis-connection-draft";
      axisId: AxisIdentifier;
      draft: Partial<AxisConnectionDraft>;
    }
  | {
      type: "workspace/sync-axis-connection-drafts";
      axes: AxisState[];
    };

const initialState: WorkspaceState = {
  activeSection: "axes",
  selectedAxisId: "axis-1",
  selectedRegisterGroupId: null,
  selectedRegisterKey: null,
  commandPaletteOpen: false,
  commandQuery: "",
  diagnosticsDockOpen: false,
  jogStepDeg: 1,
  snapshot: null,
  registerGroupsByAxis: {},
  registerErrorsByAxis: {},
  axisConnectionDrafts: {},
};

function workspaceReducer(state: WorkspaceState, action: WorkspaceAction): WorkspaceState {
  switch (action.type) {
    case "workspace/set-snapshot":
      return {
        ...state,
        snapshot: action.snapshot,
      };
    case "workspace/set-axis-registers":
      return {
        ...state,
        registerGroupsByAxis: {
          ...state.registerGroupsByAxis,
          [action.axisId]: action.groups,
        },
        registerErrorsByAxis: {
          ...state.registerErrorsByAxis,
          [action.axisId]: action.error,
        },
      };
    case "workspace/select-section":
      return {
        ...state,
        activeSection: action.section,
      };
    case "workspace/select-axis":
      return {
        ...state,
        selectedAxisId: action.axisId,
      };
    case "workspace/select-register":
      return {
        ...state,
        selectedRegisterGroupId: action.groupId,
        selectedRegisterKey: action.registerKey,
      };
    case "workspace/set-command-palette":
      return {
        ...state,
        commandPaletteOpen: action.open,
        commandQuery: action.open ? state.commandQuery : "",
      };
    case "workspace/set-command-query":
      return {
        ...state,
        commandQuery: action.query,
      };
    case "workspace/toggle-diagnostics-dock":
      return {
        ...state,
        diagnosticsDockOpen: !state.diagnosticsDockOpen,
      };
    case "workspace/set-jog-step":
      return {
        ...state,
        jogStepDeg: action.stepDeg,
      };
    case "workspace/update-axis-connection-draft": {
      const currentDraft = state.axisConnectionDrafts[action.axisId] ?? {
        devicePath: "",
        baudRate: 115200,
      };

      return {
        ...state,
        axisConnectionDrafts: {
          ...state.axisConnectionDrafts,
          [action.axisId]: {
            ...currentDraft,
            ...action.draft,
          },
        },
      };
    }
    case "workspace/sync-axis-connection-drafts": {
      const drafts = { ...state.axisConnectionDrafts };

      for (const axis of action.axes) {
        const currentDraft = drafts[axis.id];
        if (!currentDraft) {
          drafts[axis.id] = {
            devicePath: axis.devicePath ?? "",
            baudRate: axis.baudRate,
          };
          continue;
        }

        if (axis.connected) {
          drafts[axis.id] = {
            devicePath: axis.devicePath ?? currentDraft.devicePath,
            baudRate: axis.baudRate,
          };
        }
      }

      return {
        ...state,
        axisConnectionDrafts: drafts,
      };
    }
    default:
      return state;
  }
}

function findRegister(
  groups: RegisterGroup[] | undefined,
  groupId: string | null,
  registerKey: string | null,
) {
  if (!groups || !groupId || !registerKey) {
    return null;
  }

  const selectedGroup = groups.find((group) => group.id === groupId);
  return selectedGroup?.registers.find((entry) => entry.key === registerKey) ?? null;
}

export function useRobertaWorkspace() {
  const [state, dispatch] = useReducer(workspaceReducer, initialState);
  const [service] = useState<RobotService>(() => createLiveRobotService());

  useEffect(() => {
    let cancelled = false;

    const refresh = async () => {
      const snapshot = await service.getWorkspaceSnapshot();

      if (cancelled) {
        return;
      }

      startTransition(() => {
        dispatch({ type: "workspace/set-snapshot", snapshot });
      });
    };

    void refresh();
    const intervalId = window.setInterval(() => {
      void refresh();
    }, 1000);

    return () => {
      cancelled = true;
      window.clearInterval(intervalId);
    };
  }, [service]);

  const selectedAxis = useMemo(() => {
    return (
      state.snapshot?.axes.find((axis) => axis.id === state.selectedAxisId) ??
      state.snapshot?.axes[0] ??
      null
    );
  }, [state.selectedAxisId, state.snapshot]);

  const selectedAxisRegisterGroups = useMemo(() => {
    return state.registerGroupsByAxis[state.selectedAxisId] ?? [];
  }, [state.registerGroupsByAxis, state.selectedAxisId]);

  const selectedAxisRegisterError = useMemo(() => {
    return state.registerErrorsByAxis[state.selectedAxisId] ?? null;
  }, [state.registerErrorsByAxis, state.selectedAxisId]);

  const selectedRegister = useMemo(() => {
    return findRegister(
      selectedAxisRegisterGroups,
      state.selectedRegisterGroupId,
      state.selectedRegisterKey,
    );
  }, [
    selectedAxisRegisterGroups,
    state.selectedRegisterGroupId,
    state.selectedRegisterKey,
  ]);

  useEffect(() => {
    if (!state.snapshot) {
      return;
    }

    const snapshot = state.snapshot;

    startTransition(() => {
      dispatch({
        type: "workspace/sync-axis-connection-drafts",
        axes: snapshot.axes,
      });
    });
  }, [state.snapshot]);

  useEffect(() => {
    if (!state.snapshot) {
      return;
    }

    const axisExists = state.snapshot.axes.some((axis) => axis.id === state.selectedAxisId);

    if (!axisExists && state.snapshot.axes[0]) {
      dispatch({
        type: "workspace/select-axis",
        axisId: state.snapshot.axes[0].id,
      });
    }
  }, [state.selectedAxisId, state.snapshot]);

  useEffect(() => {
    const firstGroup = selectedAxisRegisterGroups[0];
    const firstRegister = firstGroup?.registers[0];

    if (!firstGroup || !firstRegister) {
      return;
    }

    const currentRegister = findRegister(
      selectedAxisRegisterGroups,
      state.selectedRegisterGroupId,
      state.selectedRegisterKey,
    );

    if (!currentRegister) {
      dispatch({
        type: "workspace/select-register",
        groupId: firstGroup.id,
        registerKey: firstRegister.key,
      });
    }
  }, [
    selectedAxisRegisterGroups,
    state.selectedRegisterGroupId,
    state.selectedRegisterKey,
  ]);

  useEffect(() => {
    let cancelled = false;

    if (!selectedAxis?.connected) {
      startTransition(() => {
        dispatch({
          type: "workspace/set-axis-registers",
          axisId: state.selectedAxisId,
          groups: [],
          error: selectedAxis ? "Axis controller is disconnected" : null,
        });
      });
      return;
    }

    const loadRegisters = async () => {
      const registerSnapshot = await service.getAxisRegisters(selectedAxis.id);

      if (cancelled) {
        return;
      }

      startTransition(() => {
        dispatch({
          type: "workspace/set-axis-registers",
          axisId: registerSnapshot.axisId,
          groups: buildRegisterGroups(registerSnapshot.entries),
          error: registerSnapshot.error,
        });
      });
    };

    void loadRegisters();

    let intervalId: number | undefined;
    if (state.activeSection === "registers") {
      intervalId = window.setInterval(() => {
        void loadRegisters();
      }, 1500);
    }

    return () => {
      cancelled = true;
      if (intervalId) {
        window.clearInterval(intervalId);
      }
    };
  }, [service, selectedAxis?.connected, selectedAxis?.id, state.activeSection, state.selectedAxisId]);

  const actions = {
    selectSection(section: WorkspaceSection) {
      dispatch({ type: "workspace/select-section", section });
    },
    selectAxis(axisId: AxisIdentifier) {
      dispatch({ type: "workspace/select-axis", axisId });
    },
    selectRegister(groupId: string, registerKey: string) {
      dispatch({ type: "workspace/select-register", groupId, registerKey });
    },
    setCommandPaletteOpen(open: boolean) {
      dispatch({ type: "workspace/set-command-palette", open });
    },
    setCommandQuery(query: string) {
      dispatch({ type: "workspace/set-command-query", query });
    },
    toggleDiagnosticsDock() {
      dispatch({ type: "workspace/toggle-diagnostics-dock" });
    },
    setJogStep(stepDeg: number) {
      dispatch({ type: "workspace/set-jog-step", stepDeg });
    },
    updateAxisConnectionDraft(
      axisId: AxisIdentifier,
      draft: Partial<AxisConnectionDraft>,
    ) {
      dispatch({
        type: "workspace/update-axis-connection-draft",
        axisId,
        draft,
      });
    },
    async connectAxis(axisId: AxisIdentifier) {
      const draft = state.axisConnectionDrafts[axisId] ?? {
        devicePath: "",
        baudRate: 115200,
      };

      const snapshot = await service.connectAxis(
        axisId,
        draft.devicePath,
        draft.baudRate,
      );

      startTransition(() => {
        dispatch({ type: "workspace/set-snapshot", snapshot });
      });
    },
    async disconnectAxis(axisId: AxisIdentifier) {
      const snapshot = await service.disconnectAxis(axisId);
      startTransition(() => {
        dispatch({ type: "workspace/set-snapshot", snapshot });
      });
    },
    async jogSelectedAxis(direction: JogDirection) {
      if (!selectedAxis) {
        return;
      }

      const snapshot = await service.jogAxis(selectedAxis.id, direction, state.jogStepDeg);
      startTransition(() => {
        dispatch({ type: "workspace/set-snapshot", snapshot });
      });
    },
    async clearSelectedAxisFault() {
      if (!selectedAxis) {
        return;
      }

      const snapshot = await service.clearAxisFault(selectedAxis.id);
      startTransition(() => {
        dispatch({ type: "workspace/set-snapshot", snapshot });
      });
    },
    async setSelectedAxisZero() {
      if (!selectedAxis) {
        return;
      }

      const snapshot = await service.setAxisZero(selectedAxis.id);
      startTransition(() => {
        dispatch({ type: "workspace/set-snapshot", snapshot });
      });
    },
  };

  const recentDiagnostics: DiagnosticsEvent[] = state.snapshot?.diagnostics ?? [];

  return {
    state,
    selectedAxis,
    selectedAxisRegisterGroups,
    selectedAxisRegisterError,
    selectedRegister,
    recentDiagnostics,
    actions,
  };
}
