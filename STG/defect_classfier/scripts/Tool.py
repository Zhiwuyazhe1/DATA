# coding =utf-8
# pip install chardet
import re, chardet
import os
import logging
logger = logging.getLogger(__name__)
class Tool:
    def __init__(self, tool_dir,output_dir):
        self.output_dir_origin = output_dir # output目录
        self.output_dir = output_dir # output目录
        self.Sseq_dir = tool_dir+'/Sseq/build/sseq ' # Sseq目录
        
    def set_output(self,fn):
        self.output_dir= self.output_dir_origin+fn

    def run_Sseq(self,cc_dir,defect_info,config,output,idx):
        #  ./sseq compile_commands.json  defect_info.json  config.txt output.json
        os.makedirs(os.path.dirname(output), exist_ok=True)
        Sseq_command = self.Sseq_dir + f' {cc_dir} {defect_info} {config} {output} >{os.path.dirname(output)}/log_{idx}.txt 2>&1\n'
        logger.info(f"执行 Sseq:{Sseq_command}")
        status_code = os.system(Sseq_command)
        return status_code
