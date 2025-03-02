```mermaid
stateDiagram-v2
	state ST_ROOT {
		[*] --> ST_OFF
		ST_OFF
		ST_ON
	}

	 ST_OFF --> ST_ON : EV_ON
	 ST_ON --> ST_OFF : EV_OFF
	 ST_ROOT : LED fsm
```