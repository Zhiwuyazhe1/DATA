import os
# ========== 核心配置==========
# 工具所在目录 = /Slicer
ROOT_PATH = os.path.abspath(os.path.dirname(os.path.dirname((os.path.dirname(__file__)))))
TOOL_DIR = ROOT_PATH
COMPILE_INFO_DIR = os.path.join(ROOT_PATH, 'compile_info')
DSC_PATH = os.path.join(os.path.join(ROOT_PATH,"defect_classfier"),"scripts")
# 脚本所在目录 = /scripts
SCRIPTS_PATH = os.path.dirname(__file__)
# 公共头文件
COMMON_FILE_NAME = 'common.h'
COMMON_FILE_PATH = os.path.join(SCRIPTS_PATH, COMMON_FILE_NAME)
# 输出目录 = /output
OUTPUT_DIR = os.path.join(ROOT_PATH, 'output')
# 映射文件路径
LINE_COUNT_FILE = os.path.join(OUTPUT_DIR, 'str_num_mapping.json')
SUCCESS_TYPE_FILE = os.path.join(OUTPUT_DIR, 'success_type_mapping.json')

# =========编译相关路径=========
LLVM_dir = '/usr/bin' #local test
link_command = LLVM_dir +os.sep+ 'llvm-link '
compile_command = LLVM_dir +os.sep
INPUT_DIR = os.path.join(ROOT_PATH,'inputs')
CC_dir= os.path.join(INPUT_DIR,'compile_commands.json')  # 编译原始文件用的
CC_dir_2= os.path.join(COMPILE_INFO_DIR,'compile_commands.json') #编译切片后
INFO_dir= os.path.join(INPUT_DIR,'input-instsInfo.txt')

# =========系统命令执行成功状态=========
SUCCESS_CODE = 0
VALIDATE_FAIL_CODE = 1
IR_FAIL_CODE = 2

# =========切片结果=========
LEVEL_0 = 'WARNING_POINTER_BACKWARD'  # 告警行后向切片，成功复现
LEVEL_1 = 'WARNING_POINTER_FORWARD'  # 告警行前向切片，成功复现
LEVEL_2 = 'ALL_EVENTS_BACKWARD'  # 全路径后向切片，成功复现
LEVEL_3 = 'ALL_EVENTS_FORWARD'  # 全路径前向切片，成功复现
LEVEL_FAIL = 'FAIL'  # 切片失败
LEVEL_RESULTS = [LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3]