import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  Input,
} from "@yourorg/ui";

export type CommandPaletteCommand = {
  id: string;
  label: string;
  description: string;
  keywords: string[];
  action: () => void;
};

type CommandPaletteProps = {
  commands: CommandPaletteCommand[];
  open: boolean;
  query: string;
  onOpenChange: (open: boolean) => void;
  onQueryChange: (query: string) => void;
};

export function CommandPalette({
  commands,
  open,
  query,
  onOpenChange,
  onQueryChange,
}: CommandPaletteProps) {
  const normalizedQuery = query.trim().toLowerCase();
  const filteredCommands = normalizedQuery
    ? commands.filter((command) => {
        const haystack = [command.label, command.description, ...command.keywords]
          .join(" ")
          .toLowerCase();

        return haystack.includes(normalizedQuery);
      })
    : commands;

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="roberta-command-palette">
        <DialogHeader>
          <DialogTitle>Command palette</DialogTitle>
          <DialogDescription>
            Trigger workspace actions without leaving the current context.
          </DialogDescription>
        </DialogHeader>
        <Input
          aria-label="Command search"
          onChange={(event) => onQueryChange(event.target.value)}
          placeholder="Select axis 1, open registers, reset faults..."
          value={query}
        />
        <div className="roberta-command-palette__list">
          {filteredCommands.map((command) => (
            <button
              className="roberta-command-palette__item"
              key={command.id}
              onClick={() => {
                onOpenChange(false);
                onQueryChange("");
                command.action();
              }}
              type="button"
            >
              <span className="roberta-command-palette__item-label">{command.label}</span>
              <span className="roberta-command-palette__item-description">
                {command.description}
              </span>
            </button>
          ))}
          {filteredCommands.length === 0 ? (
            <div className="roberta-command-palette__empty">
              No matching commands for this workspace state.
            </div>
          ) : null}
        </div>
      </DialogContent>
    </Dialog>
  );
}
