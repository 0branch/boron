/*
  Copyright 2011 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/


//#include <jit/jit.h>


enum AsmInstructions
{
    INS_RETURN,
    INS_JMP,
    INS_JMP_T,
    INS_JMP_F,

    INS_DUP,
    INS_LOAD,
    INS_NEG,
    INS_NOT,
    INS_ABS,
    INS_FLOOR,
    INS_COS,
    INS_SIN,
    INS_SQRT,

    INS_EQ,
    INS_NE,
    INS_LT,
    INS_LE,
    INS_GT,
    INS_GE,
    INS_ADD,
    INS_SUB,
    INS_MUL,
    INS_DIV,
    INS_REM,
    INS_AND,
    INS_OR,
    INS_XOR,
    INS_MIN,
    INS_MAX,
    INS_SHL,
    INS_SHR,
    INS_POW,

    INS_COUNT
};


typedef jit_value_t (*AsmInsnFunc1)(jit_function_t, jit_value_t );
typedef jit_value_t (*AsmInsnFunc2)(jit_function_t, jit_value_t, jit_value_t);


static AsmInsnFunc1 _asmIns1[] =
{
    jit_insn_dup,
    jit_insn_load,
    jit_insn_neg,
    jit_insn_not,
    jit_insn_abs,
    jit_insn_floor,
    jit_insn_cos,
    jit_insn_sin,
    jit_insn_sqrt,
};


static AsmInsnFunc2 _asmIns2[] =
{
    jit_insn_eq,
    jit_insn_ne,
    jit_insn_lt,
    jit_insn_le,
    jit_insn_gt,
    jit_insn_ge,
    jit_insn_add,
    jit_insn_sub,
    jit_insn_mul,
    jit_insn_div,
    jit_insn_rem,
    jit_insn_and,
    jit_insn_or,
    jit_insn_xor,
    jit_insn_min,
    jit_insn_max,
    jit_insn_shl,
    jit_insn_shr,
    jit_insn_pow,
};


static void _createInsTable( UThread* ut )
{
    UAtom atoms[ INS_COUNT ];
    UAtomEntry* ent;
    UBuffer* buf;
    UIndex bufN;
    int i;

    ur_internAtoms( ut,
        "return jmp jmp-t jmp-f\n"
        "dup load neg not abs floor cos sin sqrt\n"
        "eq? ne? lt? le? gt? ge? add sub mul div rem\n"
        "and or xor min max shl shr pow\n",
        atoms );

    bufN = ur_makeBinary( ut, INS_COUNT * sizeof(UAtomEntry) );
    ur_hold( bufN );

    buf = ur_buffer( bufN );
    ent = ur_ptr(UAtomEntry, buf);
    for( i = 0; i < INS_COUNT; ++i )
    {
        ent[i].atom = atoms[i];
        ent[i].index = i;
    }
    ur_atomsSort( ent, 0, INS_COUNT - 1 );

    BT->insTable = ent;
}


static jit_type_t _jitType( const UCell* cell )
{
    if( ur_is(cell, UT_WORD) )
    {
        switch( ur_atom(cell) )
        {
            case UR_ATOM_I8:    return jit_type_sbyte;
            case UR_ATOM_U8:    return jit_type_ubyte;
            case UR_ATOM_I16:   return jit_type_short;
            case UR_ATOM_U16:   return jit_type_ushort;
            case UR_ATOM_I32:   return jit_type_int;
            case UR_ATOM_U32:   return jit_type_uint;
            case UR_ATOM_F32:   return jit_type_float32;
            case UR_ATOM_F64:   return jit_type_float64;
            case UT_INT:        return jit_type_int;
            case UT_DOUBLE:     return jit_type_float64;
        }
    }
    return 0;
}


typedef struct
{
    UThread* ut;
    UBuffer labels;     // AsmLabel
    UBuffer locals;     // AsmLocalValue
    const UCell* argStart;
    const UCell* argEnd;
    jit_function_t jf;
    jit_value_t t;      // Previous instruction result.
}
Assembler;


typedef struct
{
    jit_label_t label;
    UAtom name;
}
AsmLabel;


typedef struct
{
    jit_value_t val;
    UAtom name;
}
AsmLocalValue;


static jit_value_t _asmLocalValue( const UBuffer* arr, UAtom name )
{
    const AsmLocalValue* it  = ur_ptr(AsmLocalValue, arr);
    const AsmLocalValue* end = it + arr->used;
    while( it != end )
    {
        if( it->name == name )
            return it->val;
        ++it;
    }
    return 0;
}


static jit_value_t _asmAddLocal( Assembler* as, UAtom name, jit_type_t type )
{
    AsmLocalValue* it;
    UBuffer* arr = &as->locals;
    jit_value_t val = _asmLocalValue( arr, name );
    if( val )
        return (jit_value_get_type( val ) == type) ? val : 0;

    ur_arrExpand1( AsmLocalValue, arr, it );
    it->val  = jit_value_create( as->jf, type );
    it->name = name;
    return it->val;
}


static jit_label_t* _asmLabel( Assembler* as, UAtom name )
{
    UBuffer* arr = &as->labels;
    AsmLabel* it  = ur_ptr(AsmLabel, arr);
    AsmLabel* end = it + arr->used;
    while( it != end )
    {
        if( it->name == name )
            return &it->label;
        ++it;
    }
    ur_arrExpand1( AsmLabel, arr, it );
    it->label = jit_label_undefined;
    it->name  = name;
    return &it->label;
}


/*
  Returns zero and throws error if next value is invalid.
*/
static jit_value_t _asmNextValue( const Assembler* as, UBlockIter* bi )
{
    if( ++bi->it == bi->end )
    {
        ur_error( as->ut, UR_ERR_SCRIPT, "Missing assembly operand" );
        return 0;
    }
    switch( ur_type(bi->it) )
    {
        case UT_CHAR:
            return jit_value_create_nint_constant( as->jf, jit_type_ubyte,
                                                   ur_int(bi->it) );
        case UT_INT:
            return jit_value_create_nint_constant( as->jf, jit_type_int,
                                                   ur_int(bi->it) );
        case UT_DOUBLE:
            return jit_value_create_float64_constant( as->jf, jit_type_float64,
                                                      ur_double(bi->it) );
        case UT_WORD:
        {
            const UCell* arg;
            jit_value_t local;

            for( arg = as->argStart; arg != as->argEnd; ++arg )
            {
                if( ur_equal( as->ut, arg, bi->it ) )
                {
                    return jit_value_get_param( as->jf,
                                                (arg - as->argStart) / 2 );
                }
            }

            local = _asmLocalValue( &as->locals, ur_atom(bi->it) );
            if( local )
                return local;

            arg = ur_wordCell( as->ut, bi->it );
            if( ! arg )
                return 0;
            switch( ur_type(arg) )
            {
                case UT_LOGIC:
                    return jit_value_create_nint_constant( as->jf,
                                jit_type_ubyte, ur_logic(arg) );
                case UT_CHAR:
                    return jit_value_create_nint_constant( as->jf,
                                jit_type_ubyte, ur_int(arg) );
                case UT_INT:
                    return jit_value_create_nint_constant( as->jf,
                                jit_type_int, ur_int(arg) );
                case UT_DOUBLE:
                    return jit_value_create_float64_constant( as->jf,
                                jit_type_float64, ur_double(arg) );
            }
        }
            break;

        case UT_OPTION:
            return as->t;       // /r (previous result)
    }

    ur_error( as->ut, UR_ERR_SCRIPT, "Invalid assembly operand" );
    return 0;
}


static UAtom _asmNextWord( UBlockIter* bi )
{
    if( ++bi->it == bi->end )
        return 0;
    if( ! ur_is(bi->it, UT_WORD) )
        return 0;
    return ur_atom(bi->it);
}


#define MAX_ASM_PARAM   8

/*-cf-
    assemble
        signature   block!
        body        block!  Assembly code
    return: afunc!
    group: eval
*/
CFUNC(cfunc_assemble)
{
    Assembler as;
    UBlockIter bi;
    int opcode;
    int returnKind = JIT_TYPE_INVALID;
    jit_context_t jc = (jit_context_t) BT->jit;
    jit_function_t jf;
    jit_type_t jtype;
    jit_value_t val[2];


    if( ! jc )
    {
        _createInsTable( ut );
        BT->jit = jc = jit_context_create();
    }


    // Build signature.

    {
    jit_type_t param[ MAX_ASM_PARAM ];
    jit_type_t returnType = jit_type_void;
    int pcount = 0;

    ur_blkSlice( ut, &bi, a1 );
    as.argStart = bi.it;
    as.argEnd   = bi.end;
    ur_foreach( bi )
    {
        if( ur_is(bi.it, UT_WORD ) )
        {
            if( pcount == MAX_ASM_PARAM )
            {
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "Too many assembly arguments" );
            }
            if( ++bi.it == bi.end )
                goto bad_sig;
            jtype = _jitType( bi.it );
            if( ! jtype )
                goto bad_sig;
            param[ pcount++ ] = jtype;
        }
        else if( ur_is(bi.it, UT_OPTION ) )     // /out
        {
            as.argEnd = bi.it;
            if( ++bi.it == bi.end )
                goto bad_sig;
            returnType = _jitType( bi.it );
            if( ! returnType )
                goto bad_sig;
            returnKind = jit_type_get_kind( returnType );
            break;
        }
        else
        {
bad_sig:
            return ur_error( ut, UR_ERR_SCRIPT, "Invalid assembly signature" );
        }
    }

    jit_context_build_start( jc );
    jtype = jit_type_create_signature( jit_abi_cdecl, returnType,
                                       param, pcount, 0 );
    jf = jit_function_create( jc, jtype );
    }


    // Build body.

    as.ut  = ut;
    as.jf  = jf;
    as.t   = 0;
    ur_arrInit( &as.locals, sizeof(AsmLocalValue), 0 );
    ur_arrInit( &as.labels, sizeof(AsmLabel), 0 );

    ur_blkSlice( ut, &bi, a2 );
    ur_foreach( bi )
    {
        if( ur_is(bi.it, UT_WORD) )
        {
            opcode = ur_atomsSearch( BT->insTable, INS_COUNT, ur_atom(bi.it) );
            switch( opcode )
            {
                case INS_RETURN:
                    if( ! (val[0] = _asmNextValue( &as, &bi )) )
                        goto trace_abandon;
                    jit_insn_return( jf, val[0] );
                    as.t = 0;
                    break;

                case INS_JMP:
                {
                    UAtom lname = _asmNextWord( &bi );
                    if( ! lname )
                        goto bad_ins;
                    jit_insn_branch( jf, _asmLabel( &as, lname ) );
                }
                    break;

                case INS_JMP_T:
                {
                    UAtom lname = _asmNextWord( &bi );
                    if( ! lname )
                        goto bad_ins;
                    jit_insn_branch_if( jf, as.t, _asmLabel( &as, lname ) );
                }
                    break;

                case INS_JMP_F:
                {
                    UAtom lname = _asmNextWord( &bi );
                    if( ! lname )
                        goto bad_ins;
                    jit_insn_branch_if_not( jf, as.t, _asmLabel( &as, lname ) );
                }
                    break;

                case INS_DUP:
                case INS_LOAD:
                case INS_NEG:
                case INS_NOT:
                case INS_ABS:
                case INS_FLOOR:
                case INS_COS:
                case INS_SIN:
                case INS_SQRT:
                    if( ! (val[0] = _asmNextValue( &as, &bi )) )
                        goto trace_abandon;
                    as.t = _asmIns1[ opcode - INS_DUP ]( jf, val[0] );
                    break;

                case INS_EQ:
                case INS_NE:
                case INS_LT:
                case INS_LE:
                case INS_GT:
                case INS_GE:
                case INS_ADD:
                case INS_SUB:
                case INS_MUL:
                case INS_DIV:
                case INS_REM:
                case INS_AND:
                case INS_OR:
                case INS_XOR:
                case INS_MIN:
                case INS_MAX:
                case INS_SHL:
                case INS_SHR:
                case INS_POW:
                    if( ! (val[0] = _asmNextValue( &as, &bi )) )
                        goto trace_abandon;
                    if( ! (val[1] = _asmNextValue( &as, &bi )) )
                        goto trace_abandon;
                    as.t = _asmIns2[ opcode - INS_EQ ]( jf, val[0], val[1] );
                    break;

                default:
                    goto bad_ins;
            }
        }
        else if( ur_is(bi.it, UT_GETWORD) )
        {
            if( ! as.t )
                goto bad_ins;
            val[0] = _asmAddLocal( &as, ur_atom(bi.it),
                                   jit_value_get_type(as.t) );
            if( ! val[0] )
            {
                ur_error( ut, UR_ERR_TYPE, "Local variable type mismatch" );
                goto trace_abandon;
            }
            jit_insn_store( jf, val[0], as.t );
        }
        else if( ur_is(bi.it, UT_SETWORD) )
        {
            jit_insn_label( jf, _asmLabel( &as, ur_atom(bi.it) ) );
        }
        else
        {
bad_ins:
            ur_error( ut, UR_ERR_SCRIPT, "Invalid assembly instruction" );
trace_abandon:
            ur_appendTrace( ut, a2->series.buf, bi.it - bi.buf->ptr.cell );
            goto abandon;
        }
    }

    if( ! jit_function_compile( jf ) )
    {
        ur_error( ut, UR_ERR_INTERNAL, "jit_function_compile failed" );
abandon:
        jit_function_abandon( jf );
        jit_context_build_end( jc );

        ur_arrFree( &as.locals );
        ur_arrFree( &as.labels );
        return UR_THROW;
    }

    jit_context_build_end( jc );

    {
    UCell tmp;
    UCellFunc* fc = (UCellFunc*) res;

    // Copy signature cell to slice off any output type so
    // boron_makeArgProgram() can be used.
    tmp = *a1;
    tmp.series.end = tmp.series.it + (as.argEnd - as.argStart);

    ur_setId(fc, UT_AFUNC);
    ur_afuncRet(fc) = returnKind;
    fc->argBufN = UR_INVALID_BUF;
    fc->m.jitFunc = jf;

    // Assign after fc fully initialized to handle recycle.
    fc->argBufN = boron_makeArgProgram( ut, &tmp, 0, 0, fc );
    }

    ur_arrFree( &as.locals );
    return UR_OK;
}


static int _asmCall( UThread* ut, UCellFunc* fcell, UCell* blkC, UCell* res )
{
    void* jitArgs[ MAX_ASM_PARAM ];
    void* jitRet;
    UCell* args;
    jit_function_t jitFunc = fcell->m.jitFunc;
    int ok;
    int type;
    int ja = 0;
    int nc = 0;


    switch( ur_afuncRet( fcell ) )
    {
        case JIT_TYPE_VOID:
            ur_setId(res, UT_UNSET);
            jitRet = 0;
            break;

        case JIT_TYPE_INT:
            ur_setId(res, UT_INT);
            jitRet = &ur_int(res);
            break;

        case JIT_TYPE_FLOAT64:
            ur_setId(res, UT_DOUBLE);
            jitRet = &ur_double(res);
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE, "Invalid afunc return type" );
    }

    assert( fcell->argBufN );

    // Run function argument program.
    {
        UCell* it;
        UCell* end;
        const UBuffer* abuf = ur_bufferE( fcell->argBufN );
        const uint8_t* pc = abuf->ptr.b;

        // Do not access fcell again since it can become invalid in boron_eval1.

        while( (ok = *pc++) < FO_end ) 
        {
            switch( ok )
            {
                case FO_clearLocal:
                    nc = *pc++;

                    if( ! (args = boron_stackPushN( ut, nc )) )
                        goto traceError;
                    end = args + nc;

                    for( it = args; it != end; ++it )
                        ur_setId(it, UT_NONE);
                    it = args;
                    break;

                case FO_fetchArg:
                    if( blkC->series.it >= blkC->series.end ) 
                        goto func_short;
                    if( ! (ok = boron_eval1( ut, blkC, it++ )) )
                        goto cleanup;
                    break;

                case FO_litArg:
                    if( blkC->series.it >= blkC->series.end ) 
                        goto func_short;
                    {
                        const UBuffer* blk = ur_bufferSer(blkC);
                        *it++ = blk->ptr.cell[ blkC->series.it++ ];
                    }
                    break;

                case FO_checkArg:
                    type = ur_type(it - 1);
                    if( type != *pc++ )
                    {
                        ur_error( ut, UR_ERR_TYPE,
                                  "function argument %d is invalid",
                                  it - args );
                        goto cleanup_trace;
                    }
                    switch( type )
                    {
                        case UT_CHAR:
                        case UT_INT:
                            jitArgs[ ja ] = &ur_int(it - 1);
                            break;
                        case UT_DOUBLE:
                            jitArgs[ ja ] = &ur_double(it - 1);
                            break;
                    }
                    ++ja;
                    break;

                case FO_nop:
                    break;

                case FO_nop2:
                    ++pc;
                    break;

                default:
                    ur_error( ut, UR_ERR_SCRIPT,
                              "Invalid afunc argument program" );
                    goto cleanup_trace;
            }
        }
    }
    assert( nc );

    if( jit_function_apply( jitFunc, jitArgs, jitRet ) )
        ok = UR_OK;
    else
        ok = ur_error( ut, UR_ERR_INTERNAL, "afunc exception" );

cleanup:

    boron_stackPopN( ut, nc );
    return ok;

func_short:

    ur_error( ut, UR_ERR_SCRIPT, "Unexpected end of block" );

cleanup_trace:

    boron_stackPopN( ut, nc );

traceError:

    ur_appendTrace( ut, blkC->series.buf, blkC->series.it-1 );
    return UR_THROW;
}


//EOF
