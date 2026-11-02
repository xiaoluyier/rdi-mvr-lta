"""XiWang v3: sequential 120-second formal-batch protocol."""

__version__ = "3.0.0"

from .objective import InteractionObjective, PlanEvaluation
from .empirical_kernel import ChildTransition, EmpiricalModalKernel
from .solver import GlobalPlanResult, solve_global_plan

__all__ = [
    "GlobalPlanResult",
    "ChildTransition",
    "EmpiricalModalKernel",
    "InteractionObjective",
    "PlanEvaluation",
    "solve_global_plan",
]
