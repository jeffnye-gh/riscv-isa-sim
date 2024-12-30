#pragma once
#include "cfg.h"
#include "config.h"
#include "decode.h"
#include "encoding.h"
#include "fesvr/option_parser.h"

#include "stf-inc/stf_writer.hpp"
#include "stf-inc/stf_record_types.hpp"

#include <cstdint>

#define EN_LOGGING 1
#if EN_LOGGING == 1
#define LOG(s) std::cout<<s<<std::endl;
#else
#define LOG(s)
#endif
// --------------------------------------------------------------------------
// Options and StfWriter handler
//
// TODO: description
//
//
// TODO: at the moment tracing is supported when 1 processor is begin simulated.
// ---------------------------------------------------------------------------
struct StfHandler
{
  // ----------------------------------------------------------------
  // singleton 
  // ----------------------------------------------------------------
  static StfHandler* getInstance() {
    if(!instance) instance = new StfHandler();
    return instance;
  }

  //If trace file name is empty early exit
  //If in trace region check for start/stop macros
  //  if stop - set state and return
  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_insn(processor_t *proc,insn_fetch_t &fetch,
                    std::string debug="")
  {
    //tracing is not being used
    if(!macro_tracing && !insn_num_tracing) return;

    //not in an active or pending trace region
    if(!in_trace_region && !pending_region) return;

    //This is always cleared
    pending_region = false; 

    if(macro_tracing) trace_macro_insn(proc,fetch,debug);
    else trace_count_insn(proc,fetch,debug);

  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_count_insn(processor_t *proc,insn_fetch_t &fetch,
                        std::string debug="")
  {
    //TODO
  }

  // ---------------------------------------------------------------- 
  // ---------------------------------------------------------------- 
  void trace_macro_insn(processor_t *proc,insn_fetch_t &fetch,
                        std::string debug="")
  {
    pending_region = false;

    // The trace file is not closed here since we support non-contigous 
    // regions
    if(is_stop_macro(fetch.insn.bits())) {
      LOG("-trace_insn "+debug+" FOUND STOP MACRO");
      in_trace_region = false;
      //Optionally exclude the trace macros from the trace
      if(!include_trace_macros)  return;
    }

    // This is the start macro, this could be the 1st overall or the
    // start of a new region. We can tell it is the 1st if stf_writer
    // has not been initialized. For start of any region we record the
    // state of the machine on entry to the region.
    if(is_start_macro(fetch.insn.bits())) {
      LOG("-trace_insn "+debug+" FOUND START MACRO");

      auto  _xlen = proc->get_xlen();
      reg_t _satp = proc->get_state()->satp->read();
      reg_t _asid = get_field(_satp, _xlen == 32 ? SATP32_ASID : SATP64_ASID);

      prog_asid = (uint64_t) (_asid & ASID_MASK);

      in_trace_region = true;

      if((bool)stf_writer == false)  {
        open_trace(proc,fetch);
      }
     
      record_machine_state(proc); 

      //Optionally exclude the trace macros from the trace
      if(!include_trace_macros)  return;
    }


    //We are in a trace region but stf_writer has not been initialized.
    //This means this is the first traceable instruction since detecting 
    //the start macro.

//    LOG("+trace_insn "+debug);

  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void record_machine_state(processor_t *proc) {

    auto const state = proc->get_state();
    stf_writer << stf::ForcePCRecord(state->pc);

    if(trace_register_state) {

      //NXPR declared in decode.h
      for(size_t x=0;x<NXPR;++x) {
        stf_writer << stf::InstRegRecord(x,  
          stf::Registers::STF_REG_TYPE::INTEGER,
          stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
          state->XPR[x]);
      }

      //NFPR declared in decode.h
      if(proc->get_flen() > 0) {
        for(size_t f=0;f<NFPR;++f) {
          stf_writer << stf::InstRegRecord(x,  
            stf::Registers::STF_REG_TYPE::FLOATING_POINT,
            stf::Registers::STF_REG_OPERAND_TYPE::REG_STATE,
            state->FPR[x]);
        }
      }
    }
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void open_trace(processor_t *proc,insn_fetch_t &fetch) {

    stf_writer.open(trace_file_name);

    std::string spike_sha = "SPIKE SHA:0";
    std::string stf_lib_sha = "STF_LIB SHA:0";

    uint32_t vMajor = 0;
    uint32_t vMinor = 0;
    uint32_t vPatch = 0;

    //The version and SHA's are written to the trace
    if(!force_zero_sha) {
      spike_sha = STF_SPIKE_GIT_SHA;
      stf_lib_sha = STF_LIB_GIT_SHA;
      vMajor = STF_SPIKE_VERSION_MAJOR;
      vMinor = STF_SPIKE_VERSION_MINOR;
      vPatch = STF_SPIKE_VERSION_PATCH;
    }

    stf_writer.addTraceInfo(stf::TraceInfoRecord(
      stf::STF_GEN::STF_GEN_SPIKE,vMajor,vMinor,vPatch,spike_sha)
    );

    stf_writer.addHeaderComment(stf_lib_sha);
    stf_writer.setISA(stf::ISA::RISCV);

    auto  _xlen = proc->get_xlen();
    if(_xlen == 64) {
      stf_writer.setHeaderIEM(stf::INST_IEM::STF_INST_IEM_RV64);
      stf_writer.setTraceFeature(stf::TRACE_FEATURES::STF_CONTAIN_RV64);
    } else if(_xlen == 32) {
      stf_writer.setHeaderIEM(stf::INST_IEM::STF_INST_IEM_RV32);
    } else if(_xlen == 128) {
      fprintf(stderr, //TODO: ? add 128 to stf_lib ?
        "-E: stf tracing is limited to RV32 and RV64, RV128 found\n");
      assert(0);
      //stf_writer.setHeaderIEM( stf::INST_IEM::STF_INST_IEM_RV128);
    } 

    stf_writer.setTraceFeature(
      stf::TRACE_FEATURES::STF_CONTAIN_PHYSICAL_ADDRESS
    );

    stf_writer.setHeaderPC(proc->get_state()->pc);
    stf_writer.finalizeHeader();
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  void close_trace() {
    stf_writer.flush();
    stf_writer.close();
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  bool in_traceable_region() { return in_trace_region || pending_region; }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  bool is_start_macro(uint32_t bits) {
    pending_region = true;
    return bits == _START_TRACE;
  }
  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
  bool is_stop_macro (uint32_t bits) {
    pending_region = false;
    return bits == _STOP_TRACE;
  }

  // ----------------------------------------------------------------
  // ----------------------------------------------------------------
//  uint64_t get_last_pc_stf() { return last_pc_stf; }
  // ----------------------------------------------------------------
  // option support methods
  // ----------------------------------------------------------------
  #define E(s) fprintf(stderr,s);
  void stf_help()
  {
    std::string header="";
    header.append(75,'-');
    //stf_trace options
    fprintf(stderr,"  %s\n",header.c_str());
    E("  STF Trace options\n");
    E("    - STF options are ignored if --stf_trace is not specified.\n");
    E("    - STF tracing supports a single cpu \n");
    fprintf(stderr,"  %s\n",header.c_str());
    E("  --stf_trace <file>     Dump an STF trace to the given file.\n");
    E("                         Use .zstf extension for compressed trace\n");
    E("                         output. Use .stf for uncompressed output\n");
    E("  --stf_macro_tracing    Enable STF tracing on START/STOP macros.\n");
    E("                         stf_macro_tracing and stf_insn_num_tracing\n");
    E("                         are exclusive.\n");
    E("                         (default false)\n");
    E("  --stf_insn_num_tracing Enable STF tracing on instruction count.\n");
    E("                         stf_macro_tracing and stf_insn_num_tracing\n");
    E("                         are exclusive.\n");
    E("                         (default false)\n");
    E("  --stf_insn_start <N>   Start STF tracing after N instructions.\n");
    E("  --stf_insn_count <N>  Terminate STF tracing after N instructions\n");
    E("                         from stf_insn_start.\n");
    E("  --stf_exit_on_stop_opc Terminate the simulation after detecting a\n");
    E("                         STOP_TRACE opcode. Using this switch\n");
    E("                         disables non-contiguous region tracing.\n");
    E("                         (default false)\n");
    E("  --stf_memrecord_size_in_bits\n");
    E("                         Write memory access size in bits instead \n");
    E("                         of bytes\n");
    E("                         (default false)\n");
    E("  --stf_trace_register_state\n");
    E("                         Include register state in the STF output\n");
    E("                         (default false)\n");
    E("  --stf_disable_memory_records\n");
    E("                         Do not add memory records to STF trace.\n");
    E("                         (default false)\n");
    E("  --stf_priv_modes <USHM|H|US|U>\n");
    E("                         Specify which privilege modes to include \n");
    E("                         in the trace.\n");
    E("                         (default USHM)\n");
    E("  --stf_force_zero_sha   Emit 0 for all SHA's in the STF header.\n");
    E("                         (default false)\n");
    E("  --stf_include_macros   Include the trace macros in the trace.\n");
    E("                         (default false)\n");
  }
  #undef E
  // -------------------------------------------------------------------------
  //TODO: option value validation is needed
  bool set_options(option_parser_t &parser,cfg_t &cfg)
  {

    parser.option(0,"stf_trace", 1, [&](const char* s) {
      trace_file_name = s;
    });

    if(!trace_file_name.empty() && cfg.nprocs() > 1 ) {
      fprintf(stderr,"STF tracing supports only a single cpu, %d specified.\n",
              (int)cfg.nprocs());
      return false;
    }

    parser.option(0,"stf_exit_on_stop_opc", 0, [&](const char UNUSED *s){
      exit_on_stop_opc = true;
    });

    parser.option(0,"stf_memrecord_size_in_bits", 0, [&](const char UNUSED *s){
      memrecord_size_in_bits = true;
    });

    parser.option(0,"stf_trace_register_state", 0, [&](const char UNUSED *s){
      trace_register_state = true;
    });

    parser.option(0,"stf_disable_memory_records", 0, [&](const char UNUSED *s){
      disable_memory_records = true;
    });

    parser.option(0,"stf_priv_modes", 1, [&](const char* s){
      priv_modes = s;
    });

    parser.option(0,"stf_force_zero_sha", 0, [&](const char UNUSED *s){
      force_zero_sha = true;
    });

    parser.option(0,"stf_macro_tracing", 0, [&](const char UNUSED *s){
      macro_tracing = true;
    });

    parser.option(0,"stf_insn_num_tracing", 0, [&](const char UNUSED *s){
      insn_num_tracing = true;
    });

    parser.option(0,"stf_insn_start", 1, [&](const char* s){
      insn_start = strtoull(s, nullptr, 0);
    });

    parser.option(0,"stf_insn_count", 1, [&](const char* s){
      insn_count = strtoull(s, nullptr, 0);
    });

    parser.option(0,"stf_include_macros", 0, [&](const char UNUSED *s){
      include_trace_macros = true;
    });


    //TODO add option checks
    return true;
  }

  // more singleton 
  static StfHandler *instance;

//TODO  move what can be moved to private behind accessors
//TODO  standardize the name of the member variables vs. option names
public:
  std::string trace_file_name{""};
  bool exit_on_stop_opc{false};
  bool memrecord_size_in_bits{false};
  bool trace_register_state{false};
  bool disable_memory_records{false};

  bool     force_zero_sha{false};
  bool     include_trace_macros{false};

  bool     macro_tracing{false};

  bool     insn_num_tracing{false};
  uint64_t insn_start{0};
  uint64_t insn_count{UINT64_MAX};

  std::string priv_modes{"USHM"};

  stf::STFWriter stf_writer;

  //Run time options
  uint32_t highest_priv_mode{0};
  int64_t  prog_asid{-1};

  //this flag is used to exit fast loop and enter slow loop
  //This is set when start macro has been detected
  bool     pending_region{false};

  bool     trace_file_open{false};
  bool     in_trace_region{false};  //either between start/stop macros
                                    //or in the range insn_start
                                    //and insn_start+insn_count
//  bool     stf_has_exit_pending{false};
  uint64_t num_traced{0};
  uint64_t current_insn_count{0};

//  uint64_t last_pc_stf{0};
  uint64_t last_bits_stf{0};
  uint64_t executions_stf{0};

private:
  static constexpr uint32_t _START_TRACE = 0x00004033; //xor x0,x0,x0
  static constexpr uint32_t _STOP_TRACE  = 0x0010c033; //xor x0,x1,x1
  static constexpr uint64_t ASID_MASK = 0x000000000000FFFF;
  // ----------------------------------------------------------------
  // more singleton 
  // ----------------------------------------------------------------
  StfHandler() {} //default
  StfHandler(const StfHandler&) = delete; //copy
  StfHandler(StfHandler&&)      = delete; //move
  StfHandler& operator=(const StfHandler&) = delete; //assignment

};

extern std::shared_ptr<StfHandler> stfhandler;
