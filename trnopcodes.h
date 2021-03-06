#ifndef TRNOPCODES_H
#define TRNOPCODES_H

class TrnOpcodes {
public:
    typedef enum {
        NOP,
        LDA,
        LDX,
        LDI,
        STA,
        STX,
        STI,
        ENA,
        PSH,
        POP,
        INA,
        INX = INA,
        INI = INA,
        DCA = INA,
        DCX = INA,
        DCI = INA,
        ENI,
        LSP,
        ADA,
        SUB,
        AND,
        ORA,
        XOR,
        CMA,
        JMP,
        JPN,
        JAG,
        JPZ,
        JPO,
        JSR,
        JIG,
        SHAL,
        SHAR = SHAL,
        SHXL = SHAL,
        SHXR = SHAL,
        SSP,
        SAXL,
        SAXR = SAXL,
        INP,
        OUT = INP,
        RET,
        HLT,
    } TrnOpcode;
};
#endif // TRNOPCODES_H
