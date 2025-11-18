#### Public task creation functions
Implement diffrent ways to create and add tasks.

mj_scheduler_task_add takes an already-allocated `mj_task*` rather than exposing a helper to construct tasks. This is fine, but it puts more burden on users to follow ownership rules correctly.


#### Handle non-heap ctx pointers
mj_scheduler_task_remove_current assumes that `ctx` is always heap-allocated and safe to `free`. This is correct for the demo, but the API doesn’t clearly communicate this contract.

#### Get functions for scheduler info 
`MAX_TASKS` is a global compile-time limit with no way to query capacity or current number of tasks from outside.