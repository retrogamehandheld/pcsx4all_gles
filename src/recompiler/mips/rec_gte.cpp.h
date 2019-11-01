/******************************************************************************
 * IMPORTANT: The following host registers have unique usage restrictions.    *
 *            See notes in mips_codegen.h for full details.                   *
 *  MIPSREG_AT, MIPSREG_V0, MIPSREG_V1, MIPSREG_RA                            *
 *****************************************************************************/

/* Compile-time options (disable for debugging) */

// Generate inline memory access for LWC2/SWC2 or call psxMemRead/Write C
//  functions. This feature assumes that all addresses accessed are in PS1
//  RAM or scratchpad, which should be safe. It depends on a mapped and
//  mirrored virtual address space.
#ifdef USE_DIRECT_MEM_ACCESS
#define USE_GTE_DIRECT_MEM_ACCESS
#endif

// Emit pipeline-friendly code for LWC2/SWC2 that avoids load stalls.
#define USE_GTE_MEM_PIPELINING

// Skip unnecessary write-back of GTE regs in emitMFC2(). The interpreter does
//  this in its MFC2, and from review of GTE code it seems pointless and just
//  creates load stalls.
#define SKIP_MFC2_WRITEBACK


/* Emit code to call a GTE func that takes no arguments */
#define CP2_FUNC_0(f) \
extern void gte##f(); \
void rec##f() \
{ \
	JAL(gte##f); \
	NOP(); /* <BD slot> */ \
}

/* Emit code to call a GTE func that takes one argument, which is the 32-bit
 *  opcode shifted right 10, from which it gets various parameters. No more than
 *  16 of the LSBs of the argument are used, so it can be passed with LI16.
 */
#define CP2_FUNC_1(f) \
extern void gte##f(u32 gteop); \
void rec##f() \
{ \
	JAL(gte##f); \
	LI16(MIPSREG_A0, (u16)(psxRegs.code >> 10)); /* <BD slot> */ \
}

CP2_FUNC_0(RTPS)
CP2_FUNC_0(NCLIP)
CP2_FUNC_0(NCDS)
CP2_FUNC_0(NCDT)
CP2_FUNC_0(CDP)
CP2_FUNC_0(NCCS)
CP2_FUNC_0(CC)
CP2_FUNC_0(NCS)
CP2_FUNC_0(NCT)
CP2_FUNC_0(DPCT)
CP2_FUNC_0(AVSZ3)
CP2_FUNC_0(AVSZ4)
CP2_FUNC_0(RTPT)
CP2_FUNC_0(NCCT)
CP2_FUNC_1(OP)
CP2_FUNC_1(DPCS)
CP2_FUNC_1(INTPL)
CP2_FUNC_1(MVMVA)
CP2_FUNC_1(SQR)
CP2_FUNC_1(DCPL)
CP2_FUNC_1(GPF)
CP2_FUNC_1(GPL)

static void recCFC2()
{
	if (!_Rt_) return;

	SetUndef(_Rt_);
	u32 rt = regMipsToHost(_Rt_, REG_FIND, REG_REGISTER);

	LW(rt, PERM_REG_1, offCP2C(_Rd_));
	regMipsChanged(_Rt_);
	regUnlock(rt);
}

static void emitCTC2(u32 rt, u32 reg)
{
	switch (reg) {
	case 4: case 12: case 20: case 26: case 27: case 29: case 30:
#ifdef HAVE_MIPS32R2_SEB_SEH
		SEH(TEMP_1, rt);
#else
		SLL(TEMP_1, rt, 16);
		SRA(TEMP_1, TEMP_1, 16);
#endif
		SW(TEMP_1, PERM_REG_1, offCP2C(reg));
		break;

	case 31:
		//value = value & 0x7ffff000;
		//if (value & 0x7f87e000) value |= 0x80000000;
		LI32(TEMP_1, 0x7ffff000);
		AND(TEMP_1, rt, TEMP_1);	// $t0 = rt & $t0
		LI32(TEMP_2, 0x7f87e000);
		AND(TEMP_2, TEMP_1, TEMP_2);	// $t1 = $t0 & $t2
		LUI(TEMP_3, 0x8000);		// $t2 = 0x80000000
		OR(TEMP_3, TEMP_1, TEMP_3);	// $t2 = $t0 | $t2
		MOVN(TEMP_1, TEMP_3, TEMP_2);   // if ($t1) $t0 = $t2

		SW(TEMP_1, PERM_REG_1, offCP2C(reg));
		break;

	default:
		SW(rt, PERM_REG_1, offCP2C(reg));
		break;
	}
}

static void recCTC2()
{
	u32 rt = regMipsToHost(_Rt_, REG_LOAD, REG_REGISTER);
	emitCTC2(rt, _Rd_);
	regUnlock(rt);
}

/* Limit rt to [min_reg .. max_reg], tmp_reg is overwritten  */
static void emitLIM(u32 rt, u32 min_reg, u32 max_reg, u32 tmp_reg)
{
	SLT(tmp_reg, rt, min_reg);    // tmp_reg = (rt < min_reg ? 1 : 0)
	MOVN(rt, min_reg, tmp_reg);   // if (tmp_reg) rt = min_reg
	SLT(tmp_reg, max_reg, rt);    // tmp_reg = (max_reg < rt ? 1 : 0)
	MOVN(rt, max_reg, tmp_reg);   // if (tmp_reg) rt = max_reg
}

/* move from cp2 reg to host rt */
static void emitMFC2(u32 rt, u32 reg)
{
	// IMPORTANT: Don't use these regs in this function, as they are
	//            reserved for use by LWC2/SWC2 emitter which calls here.
	// TEMP_0, MIPSREG_A0, MIPSREG_A1, MIPSREG_A2, MIPSREG_A3

	switch (reg) {
	case 1: case 3: case 5: case 8: case 9: case 10: case 11:
		LH(rt, PERM_REG_1, off(CP2D.r[reg]));
#ifndef SKIP_MFC2_WRITEBACK
		SW(rt, PERM_REG_1, off(CP2D.r[reg]));
#endif
		break;

	case 7: case 16: case 17: case 18: case 19:
		LHU(rt, PERM_REG_1, off(CP2D.r[reg]));
#ifndef SKIP_MFC2_WRITEBACK
		SW(rt, PERM_REG_1, off(CP2D.r[reg]));
#endif
		break;

	case 15:
		LW(rt, PERM_REG_1, off(CP2D.r[14])); // gteSXY2
#ifndef SKIP_MFC2_WRITEBACK
		SW(rt, PERM_REG_1, off(CP2D.r[reg]));
#endif
		break;

	//senquack - Applied fix, see comment in gte.cpp gtecalcMFC2()
	case 28: case 29:
		/* NOTE: We skip the reg assignment here and just return result.
		psxRegs.CP2D.r[reg] = LIM(gteIR1 >> 7, 0x1f, 0, 0) |
		                     (LIM(gteIR2 >> 7, 0x1f, 0, 0) << 5) |
		                     (LIM(gteIR3 >> 7, 0x1f, 0, 0) << 10);
		*/

		{
			// If you need to test this, 'Tomb Raider 5' uses it for player's
			//  model lighting.

			// XXX - If we ever decide to use more temp regs, we can come back
			//       and eliminate a load stall or two here. gteIR3 could be
			//       loaded up-front after gteIR1,2.

			const u32 lim_temp_reg = TEMP_2;
			const u32 lim_max_reg  = TEMP_3;

			LH(rt,     PERM_REG_1, off(CP2D.p[9].sw.l));  // gteIR1
			LH(TEMP_1, PERM_REG_1, off(CP2D.p[10].sw.l)); // gteIR2

			// After the right-shift, we clamp the components to 0..1f
			LI16(lim_max_reg, 0x1f);                     // lim_max_reg is upper limit

			// gteIR1:
			SRA(rt, rt, 7);
			emitLIM(rt, 0, lim_max_reg, lim_temp_reg);     // lim_temp_reg is overwritten temp reg

			// gteIR2:
			SRA(TEMP_1, TEMP_1, 7);
			emitLIM(TEMP_1, 0, lim_max_reg, lim_temp_reg); // lim_temp_reg is overwritten temp reg
			// Load gteIR3 value into whatever reg is available to reduce load stall later
			LH(lim_temp_reg, PERM_REG_1, off(CP2D.p[11].sw.l)); // gteIR3
#ifdef HAVE_MIPS32R2_EXT_INS
			INS(rt, TEMP_1, 5, 5);
#else
			SLL(TEMP_1, TEMP_1, 5);
			OR(rt, rt, TEMP_1);
#endif

			//gteIR3:
			// We loaded the value into 'lim_temp_reg' during gteIR2 sequence
			SRA(TEMP_1, lim_temp_reg, 7);
			emitLIM(TEMP_1, 0, lim_max_reg, lim_temp_reg); // lim_temp_reg is overwritten temp reg
#ifdef HAVE_MIPS32R2_EXT_INS
			INS(rt, TEMP_1, 10, 5);
#else
			SLL(TEMP_1, TEMP_1, 10);
			OR(rt, rt, TEMP_1);
#endif

#ifndef SKIP_MFC2_WRITEBACK
			SW(rt, PERM_REG_1, off(CP2D.r[29]));
#endif
		}
		break;
	default:
		LW(rt, PERM_REG_1, off(CP2D.r[reg]));
		break;
	}
}

/* move from host rt to cp2 reg */
static void emitMTC2(u32 rt, u32 reg)
{
	// IMPORTANT: Don't use these regs in this function, as they are
	//            reserved for use by LWC2/SWC2 emitter which calls here.
	// TEMP_0, MIPSREG_A0, MIPSREG_A1, MIPSREG_A2, MIPSREG_A3

	switch (reg) {
	case 15:
		LW(TEMP_1, PERM_REG_1, off(CP2D.p[13])); // tmp_gteSXY1 = gteSXY1
		LW(TEMP_2, PERM_REG_1, off(CP2D.p[14])); // tmp_gteSXY2 = gteSXY2
		SW(rt, PERM_REG_1, off(CP2D.p[14])); // gteSXY2 = value;
		SW(rt, PERM_REG_1, off(CP2D.p[15])); // gteSXYP = value;
		SW(TEMP_1, PERM_REG_1, off(CP2D.p[12])); // gteSXY0 = tmp_gteSXY1;
		SW(TEMP_2, PERM_REG_1, off(CP2D.p[13])); // gteSXY1 = tmp_gteSXY2;
		break;

	case 28:
		SW(rt, PERM_REG_1, off(CP2D.r[reg]));

		ANDI(TEMP_1, rt, 0x1f);
		SLL(TEMP_1, TEMP_1, 7);
		// gteIR1 = ((value      ) & 0x1f) << 7;
		SW(TEMP_1, PERM_REG_1, off(CP2D.r[9]));

		ANDI(TEMP_1, rt, 0x1f << 5);
		SLL(TEMP_1, TEMP_1, 2);
		// gteIR2 = ((value >>  5) & 0x1f) << 7;
		SW(TEMP_1, PERM_REG_1, off(CP2D.r[10]));

		ANDI(TEMP_1, rt, 0x1f << 10);
		SRL(TEMP_1, TEMP_1, 3);
		// gteIR3 = ((value >> 10) & 0x1f) << 7;
		SW(TEMP_1, PERM_REG_1, off(CP2D.r[11]));
		break;

	case 30:
		SW(rt, PERM_REG_1, off(CP2D.r[30]));
		SLT(TEMP_2, rt, 0);
		NOR(TEMP_1, 0, rt); // temp_1 = rt ^ -1
		MOVZ(TEMP_1, rt, TEMP_2); // if (temp_2 == 0) temp_1 = rt
		CLZ(TEMP_1, TEMP_1);
		SW(TEMP_1, PERM_REG_1, off(CP2D.r[31]));
		break;

	//senquack - Applied fix, see comment in gte.cpp gtecalcMTC2()
	case 31:
		return;

	default:
		SW(rt, PERM_REG_1, off(CP2D.r[reg]));
		break;
	}
}

static void recMFC2()
{
	if (!_Rt_) return;

	// XXX - Fix for 'Front Mission 3' random crashes in battles:
	//  The game crashes randomly when a mech/wanger is destroyed, mostly
	//   during animations involving 'leg damage'. This is caused by MFC2
	//   opcodes being followed immediately by an ALU op that reads the
	//   dest reg of the the MFC2. MFC2 has a 1-cycle load delay, meaning
	//   the ALU op should be reading the *old* value, not the new one.
	//
	//  We detect it here and simply emit both opcodes in reversed order.
	if (!branch && (opcodeGetReads(OPCODE_AT(pc)) & (1 << _Rt_))) {
		if (opcodeIsBranchOrJump(OPCODE_AT(pc))) {
			// Probably never encountered: just print a warning for devs
			printf("%s(): WARNING: Unhandled MFC2 load-delay abuse by branch at PC %08x\n", __func__, pc);
		} else {
			// Emit the op *after* the MFC2 *before* emitting the MFC2 itself.
			const u32 code_tmp = psxRegs.code;
			psxRegs.code = OPCODE_AT(pc);
			DISASM_PSX(pc);
			DISASM_MSG("%s(): Applying MFC2 load-delay abuse fix at PC %08x\n", __func__, pc);
			pc += 4;
			recBSC[psxRegs.code>>26]();
			psxRegs.code = code_tmp;
		}
	}
	// XXX - End of 'Front Mission 3' fix

	SetUndef(_Rt_);
	u32 rt = regMipsToHost(_Rt_, REG_FIND, REG_REGISTER);

	emitMFC2(rt, _Rd_);

	regMipsChanged(_Rt_);
	regUnlock(rt);
}

static void recMTC2()
{
	u32 rt = regMipsToHost(_Rt_, REG_LOAD, REG_REGISTER);

	emitMTC2(rt, _Rd_);

	regUnlock(rt);
}

/* Count successive LWC2 and/or SWC2 opcodes that share a base reg, including
 *  any NOPs found lying between them, which will be skipped by emitter.
 */
static int count_LWC2_SWC2()
{
	int count = 0;
	u32 PC = pc;
	u32 nops_at_end = 0;
	u32 opcode = psxRegs.code;
	u32 rs = _Rs_;

	/* If in delay slot, set count to 1 */
	if (branch)
		return 1;

	/* rs should be the same, imm and rt could be different */
	while (opcode == 0 ||
	       ((_fOp_(opcode) == 0x32 || _fOp_(opcode) == 0x3a) && rs == _fRs_(opcode)))
	{
		if (opcode == 0)
			nops_at_end++;
		else
			nops_at_end = 0;

		opcode = *(u32 *)((char *)PSXM(PC));
		PC += 4;
		count++;
	}

	/* Don't include any NOPs at the end of sequence in the reported count..
	    only count ones lying between the LWC2/SWC2 opcodes */
	count -= nops_at_end;

	return count;
}

static void gen_LWC2_SWC2()
{
	// IMPORTANT: These registers can be overwritten by code emitted
	//            by emitMFC2()/emitMTC2(), which we call here.
	// TEMP_1, TEMP_2, TEMP_3

	bool base_reg_converted = false;
	// Reg reserved for coverted base reg. *Cannot* be in above list.
	u32 base_reg = TEMP_0;
	// Temp reg overwritten during base reg conversion. Can be in above list.
	const u32 conversion_temp_reg = TEMP_1;

	// Get a count of the number of sequential LWC2 and/or SWC2 ops that all
	//  share the same base register. We can handle mixed LWC2s and SWC2s.
	int count = count_LWC2_SWC2();

	const u32 op_rs = _Rs_;
	const u32 rs = regMipsToHost(op_rs, REG_LOAD, REG_REGISTER);

	u32 PC = pc - 4;

#ifdef WITH_DISASM
	for (int i = 0; i < count-1; i++)
		DISASM_PSX(pc + i * 4);
#endif

#ifdef USE_GTE_DIRECT_MEM_ACCESS
	const bool direct_mem = psx_mem_mapped;
#else
	const bool direct_mem = false;
#endif

	if (direct_mem)
	{
#ifdef USE_GTE_MEM_PIPELINING
		// ---------------------------------------------------------
		//  Emit pipeline-friendly code that avoids load-use stalls
		// ---------------------------------------------------------
		//
		//   Examine the non-pipelined version first to understand the
		//  basics of LWC2/SWC2 operations. We use 4 temp regs to form a
		//  queue, which hold values from the first halves of each LWC2/SWC2
		//  operation. These values are either loads from RAM/scratchpad
		//  or loads of a GTE reg from the emu's register file. We wait as
		//  as long as possible to emit the second half of each operation,
		//  which is either a store to RAM/scratchpad or a store to a GTE
		//  reg in the emu's register file.
		//   Base address conversion is deferred until needed, which helps too.
		//
		// EXAMPLE OF A SERIES OF 4 LWC2 OR SWC2 OPCODES SHARING BASE REG:
		//  'mr/mw' : memory read/write
		//  'rr/rw' : GTE reg read/write (in psxRegisters struct)
		//  '---'   : load-use pipeline stall (4 cycles on jz4770 MIPS CPU)
		//
		// NON-PIPELINED LWC2 x 4: mr1, rw1, ---, ---, ---, mr2, rw2, ---, ---, ---, mr3, rw3, ---, ---, ---, mr4, rw4, ---, ---, ---
		//     PIPELINED LWC2 x 4: mr1, mr2, mr3, mr4, rw1, rw2, rw3, rw4
		//
		// NON-PIPELINED SWC2 x 4: rr1, mw1, ---, ---, ---, rr2, mw2, ---, ---, ---, rr3, mw3, ---, ---, ---, rr4, mw4, ---, ---, ---
		//     PIPELINED SWC2 x 4: rr1, rr2, rr3, rr4, mw1, mw2, mw3, mw4

		int queue_entries = 0;    // Number of entries currently in queue
		int queue_idx_beg = 0;    // Index of earliest entry in queue
		int queue_idx_end = 0;    // Index of latest entry in queue

		enum { LWC2_ENTRIES, SWC2_ENTRIES };
		int queue_entry_type = LWC2_ENTRIES;  // Opcode type currently in queue (if any)

		const int queue_capacity = 4;
		union {                   // Entries have info needed for 2nd half of each operation:
		    u8  gte_reg;          //  GTE reg (rt field) of entry's opcode (for LWC2 entries)
		    s16 imm;              //  Immediate mem offset of the entry's opcode (for SWC2 entries)
		} queue[queue_capacity];

		// The index of an entry maps directly to a fixed host reg
		//  that holds the 32-bit value waiting to be used later.
		const u8 queue_regmap[queue_capacity] = { MIPSREG_A0, MIPSREG_A1,
		                                          MIPSREG_A2, MIPSREG_A3 };

		// NOTE: Any NOPs that were included in count will be skipped
		int icount = count;
		do
		{
			const u32 opcode = OPCODE_AT(PC);
			PC += 4;

			if (_fOp_(opcode) == 0x32)
			{
				// LWC2

				// Flush queue if it holds SWC2 entries
				if (queue_entries > 0 && queue_entry_type == SWC2_ENTRIES) {
					do {
						if (!base_reg_converted) {
							base_reg = emitAddressConversion(op_rs, rs, base_reg, conversion_temp_reg);
							base_reg_converted = true;
						}
						const u8  entry_reg = queue_regmap[queue_idx_beg];
						const s16 entry_imm = queue[queue_idx_beg].imm;
						queue_idx_beg = (queue_idx_beg + 1) % queue_capacity;

						// Second half of a SWC2 code sequence
						SW(entry_reg, base_reg, entry_imm);
					} while (--queue_entries);
				}

				// Evict earliest queue entry if it is full
				if (queue_entries == queue_capacity) {
					const u8 entry_reg = queue_regmap[queue_idx_beg];
					const u8 gte_reg   = queue[queue_idx_beg].gte_reg;
					queue_idx_beg = (queue_idx_beg + 1) % queue_capacity;
					queue_entries--;

					// Second half of a LWC2 code sequence
					emitMTC2(entry_reg, gte_reg);
				}

				// Add this LWC2 to the queue
				{
					if (!base_reg_converted) {
						// base_reg = converted 'rs' base reg, base_reg_temp used as temp reg
						base_reg = emitAddressConversion(op_rs, rs, base_reg, conversion_temp_reg);
						base_reg_converted = true;
					}
					const u8 entry_reg = queue_regmap[queue_idx_end];
					queue[queue_idx_end].gte_reg = _fRt_(opcode);
					queue_idx_end = (queue_idx_end + 1) % queue_capacity;
					queue_entry_type = LWC2_ENTRIES;
					queue_entries++;

					// First half of a LWC2 code sequence
					LW(entry_reg, base_reg, _fImm_(opcode));
				}

				// Flush queue if this is the last opcode in the series
				if (icount == 1) {
					do {
						const u8 entry_reg = queue_regmap[queue_idx_beg];
						const u8 gte_reg   = queue[queue_idx_beg].gte_reg;
						queue_idx_beg = (queue_idx_beg + 1) % queue_capacity;

						// Second half of a LWC2 code sequence
						emitMTC2(entry_reg, gte_reg);
					} while (--queue_entries);
				}
			} else if (_fOp_(opcode) == 0x3a)
			{
				// SWC2

				// Flush queue if it holds LWC2 entries
				if (queue_entries > 0 && queue_entry_type == LWC2_ENTRIES) {
					do {
						const u8 entry_reg = queue_regmap[queue_idx_beg];
						const u8 gte_reg   = queue[queue_idx_beg].gte_reg;
						queue_idx_beg = (queue_idx_beg + 1) % queue_capacity;

						// Second half of a LWC2 code sequence
						emitMTC2(entry_reg, gte_reg);
					} while (--queue_entries);
				}

				// Evict earliest queue entry if it is full
				if (queue_entries == queue_capacity) {
					if (!base_reg_converted) {
						// base_reg = converted 'rs' base reg, base_reg_temp used as temp reg
						base_reg = emitAddressConversion(op_rs, rs, base_reg, conversion_temp_reg);
						base_reg_converted = true;
					}
					const u8  entry_reg = queue_regmap[queue_idx_beg];
					const s16 entry_imm = queue[queue_idx_beg].imm;
					queue_idx_beg = (queue_idx_beg + 1) % queue_capacity;
					queue_entries--;

					// Second half of a SWC2 code sequence
					SW(entry_reg, base_reg, entry_imm);
				}

				// Add this SWC2 to the queue
				{
					const u8 entry_reg = queue_regmap[queue_idx_end];
					queue[queue_idx_end].imm = _fImm_(opcode);
					queue_idx_end = (queue_idx_end + 1) % queue_capacity;
					queue_entry_type = SWC2_ENTRIES;
					queue_entries++;

					// First half of a SWC2 code sequence
					emitMFC2(entry_reg, _fRt_(opcode));
				}

				// Flush queue if this is the last opcode in the series
				if (icount == 1) {
					do {
						if (!base_reg_converted) {
							// base_reg = converted 'rs' base reg, base_reg_temp used as temp reg
							base_reg = emitAddressConversion(op_rs, rs, base_reg, conversion_temp_reg);
							base_reg_converted = true;
						}
						const u8  entry_reg = queue_regmap[queue_idx_beg];
						const s16 entry_imm = queue[queue_idx_beg].imm;
						queue_idx_beg = (queue_idx_beg + 1) % queue_capacity;

						// Second half of a SWC2 code sequence
						SW(entry_reg, base_reg, entry_imm);
					} while (--queue_entries);
				}
			}
		} while (--icount);

#else
		// -----------------------------
		//  Emit simple direct-mem code
		// -----------------------------

		if (!base_reg_converted) {
			base_reg = emitAddressConversion(op_rs, rs, base_reg, conversion_temp_reg);
			base_reg_converted = true;
		}

		// NOTE: Any NOPs that were included in count will be skipped
		do {
			const u32 opcode = OPCODE_AT(PC);
			PC += 4;

			// Any NOPs that were included in count will be skipped
			if (_fOp_(opcode) == 0x32) {
				// LWC2
				LW(MIPSREG_A1, base_reg, _fImm_(opcode));
				emitMTC2(MIPSREG_A1, _fRt_(opcode));
			} else if (_fOp_(opcode) == 0x3a) {
				// SWC2
				emitMFC2(MIPSREG_A1, _fRt_(opcode));
				SW(MIPSREG_A1, base_reg, _fImm_(opcode));
			}
		} while (--count);

#endif // USE_GTE_MEM_PIPELINING
	} else
	{
		// -----------------------------------------
		//  Emit code that calls C to access memory
		// -----------------------------------------

		// NOTE: Any NOPs that were included in count will be skipped
		do {
			const u32 opcode = OPCODE_AT(PC);
			PC += 4;

			if (_fOp_(opcode) == 0x32) {
				// LWC2
				JAL(psxMemRead32);                     // Read value from memory
				ADDIU(MIPSREG_A0, rs, _fImm_(opcode)); // <BD>
				emitMTC2(MIPSREG_V0, _fRt_(opcode));   // Move value read to GTE reg
			} else if (_fOp_(opcode) == 0x3a) {
				// SWC2
				emitMFC2(MIPSREG_A1, _fRt_(opcode));   // Get GTE reg value
				JAL(psxMemWrite32);                    // Write GTE reg to memory
				ADDIU(MIPSREG_A0, rs, _fImm_(opcode)); // <BD>
			}
		} while (--count);

	}

	regUnlock(rs);
	pc = PC;
}

static void recLWC2()
{
	gen_LWC2_SWC2();
}

static void recSWC2()
{
	gen_LWC2_SWC2();
}
