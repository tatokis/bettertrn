NAM  TEST
	ORG  0
step: 	LDA temp
	ADA temp2
	JPO step1
	NOP
	NOP
	NOP
step1:	LDA temp
	ADA temp2
	LDA temp2
	ADA temp2
	HLT
temp: CON  524287 
temp2: CON 1
END