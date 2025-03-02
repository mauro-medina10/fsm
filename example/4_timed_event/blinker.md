```mermaid
stateDiagram-v2
	state ROOT_ST {
		[*] --> OFF_ST
		OFF_ST
		ON_ST
	}

	 OFF_ST --> ON_ST : ON or TOGGLE
	 ON_ST --> OFF_ST : OFF  or TOGGLE

	ROOT_ST : Blinker
```