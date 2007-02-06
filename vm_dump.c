/**********************************************************************

  vm_dump.c -

  $Author$
  $Date$

  Copyright (C) 2004-2006 Koichi Sasada

**********************************************************************/


#include <ruby.h>
#include <node.h>

#include "yarvcore.h"
#include "vm.h"

#define MAX_POSBUF 128

static void
control_frame_dump(rb_thead_t *th, rb_control_frame_t *cfp)
{
    int pc = -1, bp = -1, line = 0;
    unsigned int lfp = cfp->lfp - th->stack;
    unsigned int dfp = cfp->dfp - th->stack;
    char lfp_in_heap = ' ', dfp_in_heap = ' ';
    char posbuf[MAX_POSBUF+1];

    const char *magic, *iseq_name = "-", *selfstr = "-", *biseq_name = "-";
    VALUE tmp;

    if (cfp->block_iseq != 0 && BUILTIN_TYPE(cfp->block_iseq) != T_NODE) {
	biseq_name = "";	/* RSTRING(cfp->block_iseq->name)->ptr; */
    }

    if (lfp < 0 || lfp > th->stack_size) {
	lfp = (unsigned int)cfp->lfp;
	lfp_in_heap = 'p';
    }
    if (dfp < 0 || dfp > th->stack_size) {
	dfp = (unsigned int)cfp->dfp;
	dfp_in_heap = 'p';
    }
    if (cfp->bp) {
	bp = cfp->bp - th->stack;
    }

    switch (cfp->magic) {
    case FRAME_MAGIC_TOP:
	magic = "TOP";
	break;
    case FRAME_MAGIC_METHOD:
	magic = "METHOD";
	break;
    case FRAME_MAGIC_CLASS:
	magic = "CLASS";
	break;
    case FRAME_MAGIC_BLOCK:
	magic = "BLOCK";
	break;
    case FRAME_MAGIC_FINISH:
	magic = "FINISH";
	break;
    case FRAME_MAGIC_CFUNC:
	magic = "CFUNC";
	break;
    case FRAME_MAGIC_PROC:
	magic = "PROC";
	break;
      case FRAME_MAGIC_LAMBDA:
	magic = "LAMBDA";
	break;
    case FRAME_MAGIC_IFUNC:
	magic = "IFUNC";
	break;
    case FRAME_MAGIC_EVAL:
	magic = "EVAL";
	break;
    case 0:
	magic = "------";
	break;
    default:
	magic = "(none)";
	break;
    }

    if (0) {
	tmp = rb_inspect(cfp->self);
	selfstr = StringValueCStr(tmp);
    }
    else {
	selfstr = "";
    }

    if (cfp->iseq != 0) {
	if (RUBY_VM_IFUNC_P(cfp->iseq)) {
	    iseq_name = "<ifunc>";
	}
	else {
	    pc = cfp->pc - cfp->iseq->iseq_encoded;
	    iseq_name = RSTRING_PTR(cfp->iseq->name);
	    line = th_get_sourceline(cfp);
	    if (line) {
		char fn[MAX_POSBUF+1];
		snprintf(fn, MAX_POSBUF, "%s", RSTRING_PTR(cfp->iseq->file_name));
		snprintf(posbuf, MAX_POSBUF, "%s:%d", fn, line);
	    }
	}
    }
    else if (cfp->method_id) {
	iseq_name = rb_id2name(cfp->method_id);
	snprintf(posbuf, MAX_POSBUF, ":%s", rb_id2name(cfp->method_id));
	line = -1;
    }

    fprintf(stderr, "c:%04ld ",
	    (rb_control_frame_t *)(th->stack + th->stack_size) - cfp);
    if (pc == -1) {
	fprintf(stderr, "p:---- ");
    }
    else {
	fprintf(stderr, "p:%04d ", pc);
    }
    fprintf(stderr, "s:%04ld b:%04d ", cfp->sp - th->stack, bp);
    fprintf(stderr, lfp_in_heap == ' ' ? "l:%06d " : "l:%06p ", lfp % 10000);
    fprintf(stderr, dfp_in_heap == ' ' ? "d:%06d " : "d:%06p ", dfp % 10000);
    fprintf(stderr, "%-6s ", magic);
    if (line) {
	fprintf(stderr, "%s", posbuf);
    }
    if (0) {
	fprintf(stderr, "             \t");
	fprintf(stderr, "iseq: %-24s ", iseq_name);
	fprintf(stderr, "self: %-24s ", selfstr);
	fprintf(stderr, "%-1s ", biseq_name);
    }
    fprintf(stderr, "\n");
}

void
vm_stack_dump_raw(rb_thead_t *th, rb_control_frame_t *cfp)
{
    VALUE *sp = cfp->sp, *bp = cfp->bp;
    VALUE *lfp = cfp->lfp;
    VALUE *dfp = cfp->dfp;
    VALUE *p, *st, *t;

    fprintf(stderr, "-- stack frame ------------\n");
    for (p = st = th->stack; p < sp; p++) {
	fprintf(stderr, "%04ld (%p): %08lx", p - st, p, *p);

	t = (VALUE *)*p;
	if (th->stack <= t && t < sp) {
	    fprintf(stderr, " (= %ld)", (VALUE *)GC_GUARDED_PTR_REF(t) - th->stack);
	}

	if (p == lfp)
	    fprintf(stderr, " <- lfp");
	if (p == dfp)
	    fprintf(stderr, " <- dfp");
	if (p == bp)
	    fprintf(stderr, " <- bp");	/* should not be */

	fprintf(stderr, "\n");
    }
    fprintf(stderr, "-- control frame ----------\n");
    while ((void *)cfp < (void *)(th->stack + th->stack_size)) {
	control_frame_dump(th, cfp);
	cfp++;
    }
    fprintf(stderr, "---------------------------\n");
}

void
env_dump_raw(rb_env_t *env, VALUE *lfp, VALUE *dfp)
{
    int i;
    fprintf(stderr, "-- env --------------------\n");

    while (env) {
	fprintf(stderr, "--\n");
	for (i = 0; i < env->env_size; i++) {
	    fprintf(stderr, "%04d: %08lx (%p)", -env->local_size + i, env->env[i],
		   &env->env[i]);
	    if (&env->env[i] == lfp)
		fprintf(stderr, " <- lfp");
	    if (&env->env[i] == dfp)
		fprintf(stderr, " <- dfp");
	    fprintf(stderr, "\n");
	}

	if (env->prev_envval != 0) {
	    GetEnvPtr(env->prev_envval, env);
	}
	else {
	    env = 0;
	}
    }
    fprintf(stderr, "---------------------------\n");
}

void
proc_dump_raw(rb_proc_t *proc)
{
    rb_env_t *env;
    char *selfstr;
    VALUE val = rb_inspect(proc->block.self);
    selfstr = StringValueCStr(val);

    fprintf(stderr, "-- proc -------------------\n");
    fprintf(stderr, "self: %s\n", selfstr);
    GetEnvPtr(proc->envval, env);
    env_dump_raw(env, proc->block.lfp, proc->block.dfp);
}

void
stack_dump_th(VALUE thval)
{
    rb_thead_t *th;
    GetThreadPtr(thval, th);
    vm_stack_dump_raw(th, th->cfp);
}

void
stack_dump_each(rb_thead_t *th, rb_control_frame_t *cfp)
{
    int i;

    VALUE rstr;
    VALUE *sp = cfp->sp;
    VALUE *lfp = cfp->lfp;
    VALUE *dfp = cfp->dfp;

    int argc, local_size;
    const char *name;
    rb_iseq_t *iseq = cfp->iseq;

    if (iseq == 0) {
	if (cfp->method_id) {
	    argc = 0;
	    local_size = 0;
	    name = rb_id2name(cfp->method_id);
	}
	else {
	    name = "?";
	    local_size = 0;
	}
    }
    else if (RUBY_VM_IFUNC_P(iseq)) {
	argc = 0;
	local_size = 0;
	name = "<ifunc>";
    }
    else {
	argc = iseq->argc;
	local_size = iseq->local_size;
	name = RSTRING_PTR(iseq->name);
    }

    /* stack trace header */

    if (cfp->magic == FRAME_MAGIC_METHOD ||
	cfp->magic == FRAME_MAGIC_TOP ||
	cfp->magic == FRAME_MAGIC_BLOCK ||
	cfp->magic == FRAME_MAGIC_CLASS ||
	cfp->magic == FRAME_MAGIC_PROC ||
	cfp->magic == FRAME_MAGIC_LAMBDA ||
	cfp->magic == FRAME_MAGIC_CFUNC ||
	cfp->magic == FRAME_MAGIC_IFUNC ||
	cfp->magic == FRAME_MAGIC_EVAL) {

	VALUE *ptr = dfp - local_size;

	stack_dump_each(th, cfp + 1);
	control_frame_dump(th, cfp);

	if (lfp != dfp) {
	    local_size++;
	}
	for (i = 0; i < argc; i++) {
	    rstr = rb_inspect(*ptr);
	    fprintf(stderr, "  arg   %2d: %8s (%p)\n", i, StringValueCStr(rstr),
		   ptr++);
	}
	for (; i < local_size - 1; i++) {
	    rstr = rb_inspect(*ptr);
	    fprintf(stderr, "  local %2d: %8s (%p)\n", i, StringValueCStr(rstr),
		   ptr++);
	}

	ptr = cfp->bp;
	for (; ptr < sp; ptr++, i++) {
	    if (*ptr == Qundef) {
		rstr = rb_str_new2("undef");
	    }
	    else {
		rstr = rb_inspect(*ptr);
	    }
	    fprintf(stderr, "  stack %2d: %8s (%ld)\n", i, StringValueCStr(rstr),
		   ptr - th->stack);
	}
    }
    else if (cfp->magic == FRAME_MAGIC_FINISH) {
	if ((th)->stack + (th)->stack_size > (VALUE *)(cfp + 2)) {
	    stack_dump_each(th, cfp + 1);
	}
	else {
	    /* SDR(); */
	}
    }
    else {
	rb_bug("unsupport frame type: %08lx", cfp->magic);
    }
}


void
debug_print_register(rb_thead_t *th)
{
    rb_control_frame_t *cfp = th->cfp;
    int pc = -1;
    int lfp = cfp->lfp - th->stack;
    int dfp = cfp->dfp - th->stack;
    int cfpi;

    if (RUBY_VM_NORMAL_ISEQ_P(cfp->iseq)) {
	pc = cfp->pc - cfp->iseq->iseq_encoded;
    }

    if (lfp < 0 || lfp > th->stack_size)
	lfp = -1;
    if (dfp < 0 || dfp > th->stack_size)
	dfp = -1;

    cfpi = ((rb_control_frame_t *)(th->stack + th->stack_size)) - cfp;
    fprintf(stderr, "  [PC] %04d, [SP] %04ld, [LFP] %04d, [DFP] %04d, [CFP] %04d\n",
	   pc, cfp->sp - th->stack, lfp, dfp, cfpi);
}

void
thread_dump_regs(VALUE thval)
{
    rb_thead_t *th;
    GetThreadPtr(thval, th);
    debug_print_register(th);
}

void
debug_print_pre(rb_thead_t *th, rb_control_frame_t *cfp)
{
    rb_iseq_t *iseq = cfp->iseq;

    if (iseq != 0 && cfp->magic != FRAME_MAGIC_FINISH) {
	VALUE *seq = iseq->iseq;
	int pc = cfp->pc - iseq->iseq_encoded;

	iseq_disasm_insn(0, seq, pc, iseq, 0);
    }

#if VMDEBUG > 3
    fprintf(stderr, "        (1)");
    debug_print_register(th);
#endif
}

void
debug_print_post(rb_thead_t *th, rb_control_frame_t *cfp
#if OPT_STACK_CACHING
		 , VALUE reg_a, VALUE reg_b
#endif
    )
{
#if VMDEBUG > 9
    SDR2(cfp);
#endif

#if VMDEBUG > 3
    fprintf(stderr, "        (2)");
    debug_print_register(th);
#endif
    /* stack_dump_raw(th, cfp); */

#if VMDEBUG > 2
    /* stack_dump_thobj(th); */
    stack_dump_each(th, th->cfp);
#if OPT_STACK_CACHING
    {
	VALUE rstr;
	rstr = rb_inspect(reg_a);
	fprintf(stderr, "  sc reg A: %s\n", StringValueCStr(rstr));
	rstr = rb_inspect(reg_b);
	fprintf(stderr, "  sc reg B: %s\n", StringValueCStr(rstr));
    }
#endif
    printf
	("--------------------------------------------------------------\n");
#endif

#if VMDEBUG > 9
    GC_CHECK();
#endif
}

#ifdef COLLECT_USAGE_ANALYSIS
/* uh = {
 *   insn(Fixnum) => ihash(Hash)
 * }
 * ihash = {
 *   -1(Fixnum) => count,      # insn usage
 *    0(Fixnum) => ophash,     # operand usage
 * }
 * ophash = {
 *   val(interned string) => count(Fixnum)
 * }
 */
void
vm_analysis_insn(int insn)
{
    static ID usage_hash;
    static ID bigram_hash;
    static int prev_insn = -1;

    VALUE uh;
    VALUE ihash;
    VALUE cv;

    SET_YARV_STOP();

    if (usage_hash == 0) {
	usage_hash = rb_intern("USAGE_ANALISYS_INSN");
	bigram_hash = rb_intern("USAGE_ANALISYS_INSN_BIGRAM");
    }
    uh = rb_const_get(mYarvCore, usage_hash);
    if ((ihash = rb_hash_aref(uh, INT2FIX(insn))) == Qnil) {
	ihash = rb_hash_new();
	rb_hash_aset(uh, INT2FIX(insn), ihash);
    }
    if ((cv = rb_hash_aref(ihash, INT2FIX(-1))) == Qnil) {
	cv = INT2FIX(0);
    }
    rb_hash_aset(ihash, INT2FIX(-1), INT2FIX(FIX2INT(cv) + 1));

    /* calc bigram */
    if (prev_insn != -1) {
	VALUE bi;
	VALUE ary[2];
	VALUE cv;

	ary[0] = INT2FIX(prev_insn);
	ary[1] = INT2FIX(insn);
	bi = rb_ary_new4(2, &ary[0]);

	uh = rb_const_get(mYarvCore, bigram_hash);
	if ((cv = rb_hash_aref(uh, bi)) == Qnil) {
	    cv = INT2FIX(0);
	}
	rb_hash_aset(uh, bi, INT2FIX(FIX2INT(cv) + 1));
    }
    prev_insn = insn;

    SET_YARV_START();
}

/* from disasm.c */
extern VALUE insn_operand_intern(int insn, int op_no, VALUE op,
				 int len, int pos, VALUE child);

void
vm_analysis_operand(int insn, int n, VALUE op)
{
    static ID usage_hash;

    VALUE uh;
    VALUE ihash;
    VALUE ophash;
    VALUE valstr;
    VALUE cv;

    SET_YARV_STOP();

    if (usage_hash == 0) {
	usage_hash = rb_intern("USAGE_ANALISYS_INSN");
    }

    uh = rb_const_get(mYarvCore, usage_hash);
    if ((ihash = rb_hash_aref(uh, INT2FIX(insn))) == Qnil) {
	ihash = rb_hash_new();
	rb_hash_aset(uh, INT2FIX(insn), ihash);
    }
    if ((ophash = rb_hash_aref(ihash, INT2FIX(n))) == Qnil) {
	ophash = rb_hash_new();
	rb_hash_aset(ihash, INT2FIX(n), ophash);
    }
    /* intern */
    valstr = insn_operand_intern(insn, n, op, 0, 0, 0);

    /* set count */
    if ((cv = rb_hash_aref(ophash, valstr)) == Qnil) {
	cv = INT2FIX(0);
    }
    rb_hash_aset(ophash, valstr, INT2FIX(FIX2INT(cv) + 1));

    SET_YARV_START();
}

void
vm_analysis_register(int reg, int isset)
{
    static ID usage_hash;
    VALUE uh;
    VALUE rhash;
    VALUE valstr;
    char *regstrs[] = {
	"pc",			/* 0 */
	"sp",			/* 1 */
	"cfp",			/* 2 */
	"lfp",			/* 3 */
	"dfp",			/* 4 */
	"self",			/* 5 */
	"iseq",			/* 6 */
    };
    char *getsetstr[] = {
	"get",
	"set",
    };
    static VALUE syms[sizeof(regstrs) / sizeof(regstrs[0])][2];

    VALUE cv;

    SET_YARV_STOP();

    if (usage_hash == 0) {
	char buff[0x10];
	int i;

	usage_hash = rb_intern("USAGE_ANALISYS_REGS");

	for (i = 0; i < sizeof(regstrs) / sizeof(regstrs[0]); i++) {
	    int j;
	    for (j = 0; j < 2; j++) {
		snfprintf(stderr, buff, 0x10, "%d %s %-4s", i, getsetstr[j],
			 regstrs[i]);
		syms[i][j] = ID2SYM(rb_intern(buff));
	    }
	}
    }
    valstr = syms[reg][isset];

    uh = rb_const_get(mYarvCore, usage_hash);
    if ((cv = rb_hash_aref(uh, valstr)) == Qnil) {
	cv = INT2FIX(0);
    }
    rb_hash_aset(uh, valstr, INT2FIX(FIX2INT(cv) + 1));

    SET_YARV_START();
}


#endif


VALUE
thread_dump_state(VALUE self)
{
    rb_thead_t *th;
    rb_control_frame_t *cfp;
    GetThreadPtr(self, th);
    cfp = th->cfp;

    fprintf(stderr, "Thread state dump:\n");
    fprintf(stderr, "pc : %p, sp : %p\n", cfp->pc, cfp->sp);
    fprintf(stderr, "cfp: %p, lfp: %p, dfp: %p\n", cfp, cfp->lfp, cfp->dfp);

    return Qnil;
}

void
yarv_bug()
{
    rb_thead_t *th = GET_THREAD();
    VALUE bt;

    if (GET_THREAD()->vm) {
	int i;
	SDR();
	
	bt = th_backtrace(th, 0);
	if (TYPE(bt) == T_ARRAY)
	for (i = 0; i < RARRAY_LEN(bt); i++) {
	    dp(RARRAY_PTR(bt)[i]);
	}
    }

#if HAVE_BACKTRACE
#include <execinfo.h>
#define MAX_NATIVE_TRACE 1024
    {
	static void *trace[MAX_NATIVE_TRACE];
	int n = backtrace(trace, MAX_NATIVE_TRACE);
	int i;

	fprintf(stderr, "-- backtrace of native function call (Use addr2line) --\n");
	for (i=0; i<n; i++) {
	    fprintf(stderr, "%p\n", trace[i]);
	}
	fprintf(stderr, "-------------------------------------------------------\n");
    }
#endif
}
