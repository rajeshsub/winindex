Status: Accepted

## Context

Users want to drag files out of the search result ListView and drop them onto Explorer,
the Desktop, or other drop targets. Windows uses OLE drag-and-drop for this protocol.

## Options

| Option | Fits when | Cost now | Extension path | Trade-off |
|--------|-----------|----------|----------------|-----------|
| a. OLE drag-and-drop (IDataObject + IDropSource) | Standard Windows shell integration | ~120 lines of COM boilerplate | Shell extensions, custom cursors | Correct; compatible with all Windows drop targets |
| b. Clipboard-only (Ctrl+C / Ctrl+X already implemented) | Keyboard-centric users only | Zero | None | No mouse drag; not discoverable |
| c. SHCreateDataObject / shell PIDLs | Richer shell integration (thumbnails, verbs) | More complex PIDL management | Shell context menu pass-through | Heavier; PIDL lifetimes require careful management |

## Decision

Implement **OLE drag-and-drop** (option a) using minimal hand-rolled `IDropSource` and
`IDataObject` implementations in an anonymous namespace in `MainWindow.cpp`.

The data object serves `CF_HDROP` (the standard shell file-list format), which Explorer
and all standard Windows drop targets accept for copy and move operations. `DROPEFFECT_COPY`
and `DROPEFFECT_MOVE` are both offered; the drop target decides which applies.

`OleInitialize` is called in `wWinMain` before the message loop.

## Consequences

- Files can be dragged from the result list to Explorer, the Desktop, Total Commander, etc.
- Two small COM classes (`DropSource`, `DropDataObject`) are added to `MainWindow.cpp`.
- `ole32.lib` is linked implicitly via `#pragma comment`.
- No lookaheads/backreferences syntax restriction applies (unrelated to RE2 - noted here to
  avoid confusion with ADR-0001).
