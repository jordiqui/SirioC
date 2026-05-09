"""Export SirioNNUE2 network binaries in deterministic dummy or validated-checkpoint mode."""
from __future__ import annotations
import argparse, json, struct
from pathlib import Path
from typing import Any
import torch
from .nnue2_layout_contract import *
MAGIC=b"SirioNNUE2\0\0"; VERSION=2; FEATURE_SET_ID=1; PERSPECTIVE_COUNT=2
HIDDEN_DIMENSIONS=HIDDEN1_SIZE; OUTPUT_DIMENSIONS=1

def _deterministic_i16(index:int)->int:return ((index*17+23)%2047)-1023

def build_dummy_payload():
    input_count=FEATURES_PER_PERSPECTIVE*ACCUMULATOR_SIZE; hidden_count=ACCUMULATOR_SIZE; output_count=ACCUMULATOR_SIZE
    iw=struct.pack("<"+"h"*input_count,*(_deterministic_i16(i) for i in range(input_count)))
    hb=struct.pack("<"+"h"*hidden_count,*(_deterministic_i16(100000+i) for i in range(hidden_count)))
    ow=struct.pack("<"+"h"*output_count,*(_deterministic_i16(200000+i) for i in range(output_count)))
    ob=struct.pack("<i",1337); return iw,hb,ow,ob

def build_header(sections):
    iw,hb,ow,ob=sections; payload_bytes=len(iw)+len(hb)+len(ow)+len(ob)
    return struct.pack("<12sHHHIIIIIIIIIIIII",MAGIC,VERSION,FEATURE_SET_ID,0,FEATURES_PER_PERSPECTIVE,PERSPECTIVE_COUNT,ACCUMULATOR_SIZE,HIDDEN_DIMENSIONS,OUTPUT_DIMENSIONS,QUANT_INPUT_SCALE,QUANT_OUTPUT_SCALE,len(iw),len(hb),len(ow),len(ob),payload_bytes,0)

def _as_i16(t:torch.Tensor,name:str)->bytes:
    scaled=torch.round(t).clamp(min=-32768,max=32767).to(torch.int16).contiguous().view(-1)
    return struct.pack("<"+"h"*scaled.numel(),*scaled.tolist())

def _validate(payload:dict[str,Any],cp:Path):
    for k in ("metadata","model_config","state_dict"):
        if k not in payload: raise ValueError(f"checkpoint missing {k}: {cp}")
    md,cfg,sd=payload['metadata'],payload['model_config'],payload['state_dict']
    if md.get('script_name')!=EXPECTED_SCRIPT_NAME: raise ValueError('script_name mismatch')
    if md.get('feature_set')!=FEATURE_SET: raise ValueError('feature_set mismatch')
    if int(md.get('features_per_perspective',-1))!=FEATURES_PER_PERSPECTIVE: raise ValueError('features_per_perspective mismatch')
    if md.get('model_layout_name')!=MODEL_LAYOUT_NAME: raise ValueError('unsupported model_layout_name')
    if int(md.get('model_layout_version',-1))!=MODEL_LAYOUT_VERSION: raise ValueError('unsupported model_layout_version')
    req={"input_embedding.weight":(FEATURES_PER_PERSPECTIVE,ACCUMULATOR_SIZE),"hidden.bias":(ACCUMULATOR_SIZE,),"output.weight":(1,ACCUMULATOR_SIZE),"output.bias":(1,)}
    for n,sh in req.items():
        if n not in sd: raise ValueError(f"checkpoint missing state_dict key '{n}'")
        if tuple(sd[n].shape)!=sh: raise ValueError(f"wrong tensor shape for {n}: {tuple(sd[n].shape)} != {sh}")
    return sd

def build_checkpoint_payload(checkpoint_path:str):
    payload=torch.load(Path(checkpoint_path),map_location='cpu')
    sd=_validate(payload,Path(checkpoint_path))
    iw=_as_i16(sd['input_embedding.weight'],'input_embedding.weight')
    hb=_as_i16(sd['hidden.bias'],'hidden.bias')
    ow=_as_i16(sd['output.weight'].view(-1),'output.weight')
    ob=struct.pack('<i',int(torch.round(sd['output.bias'].view(-1)[0]).item()))
    return iw,hb,ow,ob

def export(output_path:str,checkpoint_path:str|None=None):
    sections=build_dummy_payload() if checkpoint_path is None else build_checkpoint_payload(checkpoint_path)
    header=build_header(sections); out=Path(output_path); out.parent.mkdir(parents=True,exist_ok=True)
    with out.open('wb') as h:
        h.write(header); [h.write(s) for s in sections]
    return {"mode":"dummy" if checkpoint_path is None else "checkpoint","header_bytes":len(header),"payload_bytes":sum(len(s) for s in sections),"file_bytes":len(header)+sum(len(s) for s in sections)}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);p.add_argument('--checkpoint',default=None);p.add_argument('--describe',action='store_true');a=p.parse_args();stats=export(a.output,a.checkpoint)
    if a.describe: print(json.dumps(stats,indent=2))
if __name__=='__main__': main()
