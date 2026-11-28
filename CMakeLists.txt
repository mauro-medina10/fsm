set(FSM_SRCS "fsm.c")
set(FSM_REQUIRES "")

if(CONFIG_FSM_USE_FREERTOS_API OR CONFIG_FREERTOS_API)
    list(APPEND FSM_REQUIRES freertos)
else()
    list(APPEND FSM_SRCS "ring_buff.c")
endif()

idf_component_register(SRCS ${FSM_SRCS}
                       INCLUDE_DIRS "include"
                       REQUIRES ${FSM_REQUIRES})
