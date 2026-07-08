# Stochastic extension

The deterministic Bellman solver assumes the full price and inflow path is known. The first stochastic extension included in this repository is a stagewise discrete uncertainty solver:

```text
V_t(s) = max_a E_w [ r(s, a, w) + gamma V_{t+1}(f(s, a, w)) ]
```

where `w` is a discrete realization of price and inflow at time `t`.

The implementation is in:

```text
libs/optimization/include/optiflow/stochastic/StochasticTypes.h
libs/optimization/include/optiflow/stochastic/StochasticBellmanSolver.h
libs/optimization/src/stochastic/StochasticBellmanSolver.cpp
```

## Interpretation

The current stochastic solver assumes:

- the physical state is observed before action selection;
- the current exogenous realization is not observed before action selection;
- the policy depends on time and physical state only;
- the action must be feasible under every realization at that stage.

This is conservative. It is useful as a clean first stochastic DP, but it is not the only valid market interpretation.

## If price is observed before dispatch

If the operator observes price before choosing the action, price should be part of the observed state or regime. The policy then becomes:

```text
pi_t(volume, soc, price_regime, inflow_regime)
```

That makes the state space larger but gives a more realistic recourse policy.

## If uncertainty follows a Markov process

Represent price and inflow as regimes with a transition matrix:

```text
P(regime_{t+1} | regime_t)
```

Then solve over the augmented state:

```text
volume x battery_soc x regime
```

The recursion becomes:

```text
V_t(s, z) = max_a [ r(s, a, z) + gamma sum_z' P(z' | z) V_{t+1}(s', z') ]
```

## If uncertainty is scenario-tree based

For multi-stage stochastic programming, use a scenario tree where each node contains conditional probabilities and exogenous values. This is closer to stochastic programming than classic grid DP. The policy is non-anticipative: decisions at nodes sharing the same information history must be identical.

## Interview-safe description

Use this wording:

> The deterministic solver handles a known day-ahead path. The stochastic extension replaces the single exogenous path with a discrete distribution at each stage and maximizes expected Bellman value. For a more realistic market setting, I would augment the state with observed price/inflow regimes or use a scenario tree with non-anticipativity constraints.

## Known limitations

The current stochastic solver does not yet implement:

- Markov regime transitions;
- scenario-tree non-anticipativity;
- stochastic forward simulation;
- risk measures such as CVaR;
- approximate dynamic programming;
- SDDP for large hydro systems.
