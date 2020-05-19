NAM  TEST
	ORG  0
step: 	LDA temp
	INA
	JPO FIN
FIN:	LDX temp
	INX
HLT
temp: 	CON  524287,-524288
temp2: 	CON -1,5

END