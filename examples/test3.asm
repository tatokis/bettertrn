	NAM test2
	ORG 0
	LDA LOC
	LDX LOC+1
	LDI LOC+2
	INA
	INX
	INI
	SAXL
	SAXR
	DCA
	DCX
	DCI
	HLT
LOC:	CON 2,4,8
	END