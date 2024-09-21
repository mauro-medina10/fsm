::: mermaid
stateDiagram-v2
    [*] --> ROOT_ST
    state ROOT_ST {
        [*] --> INIT_ST
        INIT_ST --> OFF_ST : READY / enter_off()
        OFF_ST --> ON_ST : ON / enter_on()
        OFF_ST --> ON_ST : TOGGLE / enter_on()
        ON_ST --> OFF_ST : TOGGLE / enter_off()
        ON_ST --> OFF_ST : OFF / enter_off()
        ON_ST --> UPDATE_ST : UPDATE / enter_update()
        UPDATE_ST --> ON_ST : READY / enter_on()

        INIT_ST : Initial State
        OFF_ST : LED Off State
        ON_ST : LED On State
        UPDATE_ST : Update State
    }
    ROOT_ST : LED FSM
:::