#ifndef PCPU
#define PCPU

#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <fstream>
#include <string>

namespace ddl {

    typedef uint32_t dw;

    class Registers;
    class Memory;
    class CPU;

    class Registers {
    private:
        dw cont[32];

    public:
        Registers() {
            std::memset(cont, 0, sizeof(dw) * 32);
        }

        dw& operator[](dw index) {return cont[index];}
    };

    class Memory {
    private:
        dw pu;
        uint8_t dst[0x20000];
        //std::fstream src;

        /*
        void set_path(const std::string& pth) {
            if (src.is_open()) src.close();
            src.open(pth, std::ios::in);
        }
         */
        // char->hex
        static uint8_t ctoui(char cur) {
            return (cur>='0'&&cur<='9') ? (cur-'0') : (cur-'A'+10);
        }
        // is_hex
        static bool ishx(char cur) {
            return (cur>='0'&&cur<='9')||(cur>='A'&&cur<='F');
        }
        void read() {
            //set_path(pth);

            char cur;
            while (std::cin >> cur) {
                if (cur=='@') {
                    int cnt = 8;
                    pu = 0;
                    while (cnt--) {
                        std::cin >> cur;
                        pu <<= 4u;
                        pu += ctoui(cur);
                    }
                }
                else if (ishx(cur)) {
                    dst[pu] = ctoui(cur);
                    dst[pu] <<= 4u;
                    std::cin >> cur;
                    dst[pu] += ctoui(cur);
                    ++pu;
                }
            }

            //src.close();
        }

    public:
        Memory() : pu(0) {read();}

        dw readdw(dw index) {return *((uint32_t*)(dst + index));}
        uint16_t readwu(dw index) {return *((uint16_t*)(dst + index));}
        int16_t readws(dw index) {return *((int16_t*)(dst + index));}
        uint8_t readbu(dw index) {return *((uint8_t*)(dst + index));}
        int8_t readbs(dw index) {return *((int8_t*)(dst + index));}

        void writedw(dw index, dw value) { *((uint32_t*)(dst + index)) = value;}
        void writewu(dw index, uint16_t value) { *((uint16_t*)(dst + index)) = value;}
        void writebu(dw index, uint8_t value) { *(dst + index) = value;}
    };

    class CPU {
    private:
        Registers Reg;
        Memory Mem;
        dw PC;

    private:
        struct IF_ID {
            dw NPC;
            dw IR;
        } if_id;

        enum OP_TYPE {R, I, S, B, U, J};
        struct INST {
            dw raw_code;
            dw op_code;
            OP_TYPE op_type;
            dw func7, func3;
            dw rs1, rs2, rd;

            INST& operator=(const INST& oth) = default;
        };

        struct ID_EX {
            INST IR;

            dw rs1v, rs2v;
            dw imm;
            dw shamt;

            dw NPC;

            bool jumped;
            dw JPC;

            bool stall;

            bool exit;
        } id_ex;
        dw imm_slice(dw len, dw src, dw dst) const {
            if (src >= dst)
                return (if_id.IR & ( ((1u << len) - 1u) << src)) >> (src - dst);
            else
                return (if_id.IR & ( ((1u << len) - 1u) << src)) << (dst - src);
        }
        dw imm_slice_ar(dw len, dw src, dw dst) const {
            return (dw)((int)(if_id.IR & ( ((1u << len) - 1u) << src) ) >> (src - dst));
        }
        bool need_to_stall() const {
            return ( ((id_ex.IR.rs1&&id_ex.IR.rs1==mem_wb.IR.rd)
                        || (id_ex.IR.rs2&&id_ex.IR.rs2==mem_wb.IR.rd))
                    && (mem_wb.IR.op_type==R || mem_wb.IR.op_type==U
                        || (mem_wb.IR.op_type==I&&mem_wb.IR.op_code==19)) );
        }

        struct EX_MEM {
            bool enabled_mem;
            bool enabled_wb;

            INST IR;

            dw ALUOutput;
            dw rs2v;

            bool jumped;
            dw JPC;

            bool exit;
        } ex_mem;
        static uint8_t slice_expand_b(dw data) {
            return (uint8_t)(data & ((1u << 8u) - 1u));
        }
        static uint16_t slice_expand_w(dw data) {
            return (uint16_t)(data & ((1u << 16u) - 1u));
        }

        struct MEM_WB {
            bool enabled;

            INST IR;

            dw LMD;
            dw ALUOutput;
        } mem_wb;

        struct FORWARDING {
            bool enabled;

            dw rd;
            dw val;
        };
        FORWARDING FRWRD_EX;
        FORWARDING FRWRD_MEM;

        struct BPB {
            bool f, s;

            BPB() : f(false), s(false) {}

            BPB& operator++() {
                if (f ^ s) {f = true; s = true;}
                else if (!f && !s) {s = true;}
                return *this;
            }
            BPB& operator--() {
                if (f ^ s) {f = false; s = false;}
                else if (f && s) {s = false;}
                return *this;
            }

            bool picked() const {return f;}
        } TBPB; // 2-bit branch-prediction buffer

    public:
        CPU()
            : Reg(), Mem(), PC(0), TBPB() {
            id_ex.exit = false;
            id_ex.jumped = false;
            id_ex.stall = false;

            ex_mem.exit = false;
            ex_mem.jumped = false;
            ex_mem.enabled_mem = false;
            ex_mem.enabled_wb = false;

            mem_wb.enabled = false;

            FRWRD_EX.enabled = false;
            FRWRD_MEM.enabled = false;
        }

    protected:
        void IF();
        void ID();
        void EX();
        void MEM();
        void WB();

    public:
        void CC();
    };

    void CPU::IF() {
        if_id.NPC = PC;
        if_id.IR = Mem.readdw(PC);
        PC += 4;
    }

    void CPU::ID() {
        id_ex.IR.raw_code = if_id.IR;
        id_ex.IR.op_code = if_id.IR & 127u;
        id_ex.NPC = if_id.NPC;
        switch (id_ex.IR.op_code) {
            // R-type
            case 51: {
                id_ex.IR.op_type = R;
                id_ex.IR.func7 = imm_slice(7, 25, 0);
                id_ex.IR.func3 = imm_slice(3, 12, 0);

                id_ex.IR.rs1 = imm_slice(5, 15, 0);
                id_ex.rs1v = Reg[id_ex.IR.rs1];
                id_ex.IR.rs2 = imm_slice(5, 20, 0);
                id_ex.rs2v = Reg[id_ex.IR.rs2];

                id_ex.IR.rd = imm_slice(5, 7, 0);

                if (id_ex.stall) {id_ex.stall = false;}
                else {id_ex.stall = need_to_stall();}

                break;
            }
            // I-type
            case 103: case 3: case 19: {
                id_ex.IR.op_type = I;
                id_ex.IR.func3 = imm_slice(3, 12, 0);

                id_ex.IR.rs1 = imm_slice(5, 15, 0);
                id_ex.rs1v = Reg[id_ex.IR.rs1];

                id_ex.IR.rd = imm_slice(5, 7, 0);

                if (id_ex.IR.func3 == 1 || id_ex.IR.func3 == 5) {
                    id_ex.IR.func7 = imm_slice(7, 25, 0);
                    id_ex.shamt = imm_slice(5, 20, 0);
                }
                else {
                    id_ex.imm = imm_slice_ar(12, 20, 0);
                    // BREAK
                    if (id_ex.IR.op_code==19&&id_ex.IR.func3==0
                        &&id_ex.IR.rd==10&&id_ex.imm==255) {
                        id_ex.exit = true;
                        return;
                    }
                }
                break;
            }
            // S-type
            case 35: {
                id_ex.IR.op_type = S;
                id_ex.IR.func3 = imm_slice(3, 12, 0);

                id_ex.IR.rs1 = imm_slice(5, 15, 0);
                id_ex.rs1v = Reg[id_ex.IR.rs1];
                id_ex.imm = imm_slice(5, 7, 0)
                          | imm_slice_ar(7, 25, 5);

                id_ex.IR.rs2 = imm_slice(5, 20, 0);
                id_ex.rs2v = Reg[id_ex.IR.rs2];

                if (id_ex.stall) {id_ex.stall = false;}
                else {id_ex.stall = need_to_stall();}

                break;
            }
            // B-type
            case 99: {
                id_ex.IR.op_type = B;
                id_ex.IR.func3 = imm_slice(3, 12, 0);

                id_ex.IR.rs1 = imm_slice(5, 15, 0);
                id_ex.rs1v = Reg[id_ex.IR.rs1];
                id_ex.IR.rs2 = imm_slice(5, 20, 0);
                id_ex.rs2v = Reg[id_ex.IR.rs2];

                id_ex.imm = imm_slice(1, 7, 11)
                          | imm_slice(4, 8, 1)
                          | imm_slice(6, 25, 5)
                          | imm_slice_ar(1, 31, 12);

                if (id_ex.stall) {id_ex.stall = false;}
                else {id_ex.stall = need_to_stall();}

                if (!id_ex.stall && TBPB.picked()) {
                    id_ex.jumped = true;
                    id_ex.JPC = id_ex.NPC + id_ex.imm;
                }

                break;
            }
            // U-type
            case 55: case 23: {
                id_ex.IR.op_type = U;
                id_ex.IR.rd = imm_slice(5, 7, 0);
                id_ex.imm = imm_slice(20, 12, 12);
                break;
            }
            // J-type
            case 111: {
                id_ex.IR.op_type = J;
                id_ex.IR.rd = imm_slice(5, 7, 0);
                id_ex.imm = imm_slice(8, 12, 12)
                          | imm_slice(1, 20, 11)
                          | imm_slice(4, 21, 1)
                          | imm_slice(6, 25, 5)
                          | imm_slice_ar(1, 31, 20);

                id_ex.jumped = true;
                id_ex.JPC = id_ex.NPC + id_ex.imm;
                break;
            }
            default:;
        }
    }

    void CPU::EX() {
        ex_mem.IR = id_ex.IR;
        switch (id_ex.IR.op_type) {
            case R: {
                ex_mem.enabled_wb = true;

                // ADD / SUB
                if (id_ex.IR.func3==0) {
                    if (id_ex.IR.func7==0)
                        ex_mem.ALUOutput = (dw)((int)id_ex.rs1v + (int)id_ex.rs2v);
                    else
                        ex_mem.ALUOutput = (dw)((int)id_ex.rs1v - (int)id_ex.rs2v);
                }
                // SLL
                else if (id_ex.IR.func3==1) {
                    ex_mem.ALUOutput = id_ex.rs1v << id_ex.rs2v;
                }
                // SRL / SRA
                else if (id_ex.IR.func3==5) {
                    if (id_ex.IR.func7==0)
                        ex_mem.ALUOutput = id_ex.rs1v >> id_ex.rs2v;
                    else
                        ex_mem.ALUOutput = (dw)((int)id_ex.rs1v >> id_ex.rs2v);
                }
                // SLT
                else if (id_ex.IR.func3==2) {
                    ex_mem.ALUOutput = ((int)id_ex.rs1v < (int)id_ex.rs2v) ? 1 : 0;
                }
                // SLTU
                else if (id_ex.IR.func3==3) {
                    ex_mem.ALUOutput = (id_ex.rs1v < id_ex.rs2v) ? 1 : 0;
                }
                // XOR
                else if (id_ex.IR.func3==4) {
                    ex_mem.ALUOutput = id_ex.rs1v ^ id_ex.rs2v;
                }
                // OR
                else if (id_ex.IR.func3==6) {
                    ex_mem.ALUOutput = id_ex.rs1v | id_ex.rs2v;
                }
                // AND
                else {
                    ex_mem.ALUOutput = id_ex.rs1v & id_ex.rs2v;
                }

                FRWRD_EX.enabled = true;
                FRWRD_EX.rd = id_ex.IR.rd;
                FRWRD_EX.val = ex_mem.ALUOutput;

                break;
            }
            case I: {
                if (id_ex.IR.op_code==19) {
                    ex_mem.enabled_wb = true;

                    // ADDI
                    if (id_ex.IR.func3==0) {
                        ex_mem.ALUOutput = (dw)((int)id_ex.rs1v + (int)id_ex.imm);
                    }
                    // SLTI
                    else if (id_ex.IR.func3==2) {
                        ex_mem.ALUOutput = ((int)id_ex.rs1v < (int)id_ex.imm) ? 1 : 0;
                    }
                    // SLTIU
                    else if (id_ex.IR.func3==3) {
                        ex_mem.ALUOutput = (id_ex.rs1v < id_ex.imm) ? 1 : 0;
                    }
                    // XORI
                    else if (id_ex.IR.func3==4) {
                        ex_mem.ALUOutput = id_ex.rs1v ^ id_ex.imm;
                    }
                    // ORI
                    else if (id_ex.IR.func3==6) {
                        ex_mem.ALUOutput = id_ex.rs1v | id_ex.imm;
                    }
                    // ANDI
                    else if (id_ex.IR.func3==7) {
                        ex_mem.ALUOutput = id_ex.rs1v & id_ex.imm;
                    }
                    // SLLI
                    else if (id_ex.IR.func3==1) {
                        ex_mem.ALUOutput = id_ex.rs1v << id_ex.shamt;
                    }
                    // SRLI / SRAI
                    else {
                        if (id_ex.IR.func7==0)
                            ex_mem.ALUOutput = id_ex.rs1v >> id_ex.shamt;
                        else
                            ex_mem.ALUOutput = (dw)((int)id_ex.rs1v >> id_ex.shamt);
                    }

                    FRWRD_EX.enabled = true;
                    FRWRD_EX.rd = id_ex.IR.rd;
                    FRWRD_EX.val = ex_mem.ALUOutput;
                }
                // LOAD
                else if (id_ex.IR.op_code==3) {
                    ex_mem.enabled_wb = true;
                    ex_mem.enabled_mem = true;

                    ex_mem.ALUOutput = id_ex.rs1v + id_ex.imm;
                }
                // JALR
                else {
                    if (id_ex.IR.rd) Reg[id_ex.IR.rd] = id_ex.NPC+4;

                    PC = id_ex.rs1v + id_ex.imm;
                    PC &= (((dw)1 << (dw)31) - (dw)1) << (dw)1;
                    ex_mem.jumped = true;
                    ex_mem.JPC = PC;
                }
                break;
            }
            case S: {
                ex_mem.enabled_mem = true;

                ex_mem.ALUOutput = id_ex.rs1v + id_ex.imm;
                ex_mem.rs2v = id_ex.rs2v;
                break;
            }
            case B: {
                if (
                       (id_ex.IR.func3==0 && id_ex.rs1v == id_ex.rs2v) // BEQ
                    || (id_ex.IR.func3==1 && id_ex.rs1v != id_ex.rs2v) //BNE
                    || (id_ex.IR.func3==4 && (int)id_ex.rs1v < (int)id_ex.rs2v) // BLT
                    || (id_ex.IR.func3==5 && (int)id_ex.rs2v < (int)id_ex.rs1v) // BGE
                    || (id_ex.IR.func3==6 && id_ex.rs1v < id_ex.rs2v) // BLTU
                    || (id_ex.IR.func3==7 && id_ex.rs2v < id_ex.rs1v) // BGEU
                   ) {
                    if (!TBPB.picked()) {
                        ex_mem.jumped = true;
                        ex_mem.JPC = id_ex.NPC + id_ex.imm;
                    }
                    ++TBPB;
                }
                else {
                    if (TBPB.picked()) {
                        ex_mem.jumped = true;
                        ex_mem.JPC = id_ex.NPC + 4;
                    }
                    --TBPB;
                }
                break;
            }
            case U: {
                ex_mem.enabled_wb = true;

                // LUI
                if (id_ex.IR.op_code==55)
                    ex_mem.ALUOutput = id_ex.imm;
                // AUIPC
                else
                    ex_mem.ALUOutput = id_ex.NPC + id_ex.imm;

                FRWRD_EX.enabled = true;
                FRWRD_EX.rd = id_ex.IR.rd;
                FRWRD_EX.val = ex_mem.ALUOutput;

                break;
            }
            // JAL
            case J: {
                if (id_ex.IR.rd) Reg[id_ex.IR.rd] = id_ex.NPC+4;
                break;
            }
            default:;
        };
    }

    void CPU::MEM() {
        mem_wb.enabled = ex_mem.enabled_wb;
        ex_mem.enabled_wb = false;
        mem_wb.IR = ex_mem.IR;
        if (ex_mem.enabled_mem) {
            ex_mem.enabled_mem = false;

            // LOAD
            if (ex_mem.IR.op_type == I) {
                // LW
                if (ex_mem.IR.func3 == 2) {
                    mem_wb.LMD = Mem.readdw(ex_mem.ALUOutput);
                }
                // LH
                else if (ex_mem.IR.func3 == 1) {
                    mem_wb.LMD = Mem.readws(ex_mem.ALUOutput);
                }
                // LB
                else if (ex_mem.IR.func3 == 0) {
                    mem_wb.LMD = Mem.readbs(ex_mem.ALUOutput);
                }
                // LHU
                else if (ex_mem.IR.func3 == 5) {
                    mem_wb.LMD = Mem.readwu(ex_mem.ALUOutput);
                }
                // LBU
                else {
                    mem_wb.LMD = Mem.readbu(ex_mem.ALUOutput);
                }

                FRWRD_MEM.enabled = true;
                FRWRD_MEM.rd = mem_wb.IR.rd;
                FRWRD_MEM.val = mem_wb.LMD;
            }
            // STORE
            else {
                // SW
                if (ex_mem.IR.func3 == 2) {
                    Mem.writedw(ex_mem.ALUOutput, ex_mem.rs2v);
                }
                // SH
                else if (ex_mem.IR.func3 == 1) {
                    Mem.writewu(ex_mem.ALUOutput, slice_expand_w(ex_mem.rs2v));
                }
                // SB
                else {
                    Mem.writebu(ex_mem.ALUOutput, slice_expand_b(ex_mem.rs2v));
                }
            }
        }
        else if (mem_wb.enabled) {
            mem_wb.ALUOutput = ex_mem.ALUOutput;

            FRWRD_MEM.enabled = true;
            FRWRD_MEM.rd = mem_wb.IR.rd;
            FRWRD_MEM.val = mem_wb.ALUOutput;
        }
    }

    void CPU::WB() {
        if (mem_wb.enabled && mem_wb.IR.rd) {
            if (mem_wb.IR.op_type==I && mem_wb.IR.op_code==3)
                Reg[mem_wb.IR.rd] = mem_wb.LMD;
            else
                Reg[mem_wb.IR.rd] = mem_wb.ALUOutput;
        }
        mem_wb.enabled = false;
    }

    void CPU::CC() {
        dw if_c=0, id_c=1, ex_c=2, mem_c=3, wb_c=4;
        dw stall1 = 0, stall2 = 0;

        while (true) {
            if (wb_c) {--wb_c;}
            else if (stall1==5) {++stall1;}
            else if (stall2==5) {++stall2;}
            else WB();

            if (mem_c) {--mem_c;}
            else if (stall1==4) {++stall1;}
            else if (stall2==4) {++stall2;}
            else {
                if (ex_mem.exit) {
                    ex_mem.exit = false;
                    break;
                }
                if (ex_mem.enabled_mem) {
                    if_c = id_c = ex_c = mem_c = 3;
                    wb_c = 2;
                }
                MEM();
            }

            if (ex_c) {--ex_c;}
            else if (stall1==3) {++stall1;}
            else if (stall2==3) {++stall2;}
            else {
                EX();
                if (id_ex.exit) {
                    id_ex.exit = false;
                    ex_mem.exit = true;
                }
            }

            if (id_c) {--id_c;}
            else if (stall1==2) {++stall1;}
            else if (stall2==2) {++stall2;}
            else {
                ID();
                if (id_ex.exit) {
                    stall1 = 1;
                    stall2 = 1;
                }
            }

            if (if_c) {--if_c;}
            else if (stall1==1) {++stall1;}
            else if (stall2==1) {++stall2;}
            else IF();

            if (id_ex.stall) {
                stall1 = 3;
                if_id.NPC = id_ex.NPC;
                if_id.IR = id_ex.IR.raw_code;
                PC = if_id.NPC + 4;
            }
            if (ex_mem.jumped) {
                ex_mem.jumped = false;
                id_ex.jumped = false;
                if (id_ex.exit) id_ex.exit = false;
                if (id_ex.stall) id_ex.stall = false;

                stall1 = 3;
                stall2 = 2;
                PC = ex_mem.JPC;
            }
            if (id_ex.jumped) {
                id_ex.jumped = false;

                stall1 = 2;
                PC = id_ex.JPC;
            }

            if (!ex_c && stall1!= 3 && stall2!=3) {
                if (id_ex.IR.op_type==R || id_ex.IR.op_type==B || id_ex.IR.op_type==S) {
                    if (FRWRD_MEM.enabled && FRWRD_MEM.rd) {
                        if (FRWRD_MEM.rd == id_ex.IR.rs1) {
                            id_ex.rs1v = FRWRD_MEM.val;
                        }
                        else if (FRWRD_MEM.rd == id_ex.IR.rs2) {
                            id_ex.rs2v = FRWRD_MEM.val;
                        }
                    }
                    if (FRWRD_EX.enabled && FRWRD_EX.rd) {
                        if (FRWRD_EX.rd == id_ex.IR.rs1) {
                            id_ex.rs1v = FRWRD_EX.val;
                        }
                        else if (FRWRD_EX.rd == id_ex.IR.rs2) {
                            id_ex.rs2v = FRWRD_EX.val;
                        }
                    }
                }
                else if (id_ex.IR.op_type==I) {
                    if (FRWRD_MEM.enabled && FRWRD_MEM.rd
                        && FRWRD_MEM.rd == id_ex.IR.rs1) {
                        id_ex.rs1v = FRWRD_MEM.val;
                    }
                    if (FRWRD_EX.enabled && FRWRD_EX.rd
                        && FRWRD_EX.rd == id_ex.IR.rs1) {
                        id_ex.rs1v = FRWRD_EX.val;
                    }
                }
                FRWRD_EX.enabled = false;
                FRWRD_MEM.enabled = false;
            }
        }

        printf("%d", Reg[10] & 255u);
    }

}

#endif
