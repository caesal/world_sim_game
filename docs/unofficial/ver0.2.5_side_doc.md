# Ver0.2.5 Side Doc

Ver0.2.5 focuses on making water, route potential, and World-tab UI behavior line up with what the player sees.

## Main Changes

1. Water display now presents shallow sea and deep sea as the player-facing categories.
2. Water rendering uses a visual shallow-to-deep gradient, while gameplay keeps hard shallow/deep thresholds.
3. Route potential is precomputed from natural-region port sites after world generation.
4. Occupied port-site regions activate their corresponding potential port deterministically.
5. Visible ordinary sea lanes are activated from occupied route-potential nodes instead of random city-port generation.
6. Debug route-potential mode shows the full graph, while the ordinary map shows only active lanes.
7. World-tab random buttons and native edit controls were brought back into the dark UI style and repaint flow.
8. Map centering, side-panel collapse behavior, legend clipping, and map-size defaults were tightened.

## Validation

- `make` passed after the Ver0.2.5 code update.
- All `.c` and `.h` files remained at or below 500 lines.
- `logs/tech10_probe.txt` records the latest deterministic stage-10 probe output.
