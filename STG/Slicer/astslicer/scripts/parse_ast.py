import clang.cindex
from clang.cindex import Index, Config, CursorKind, TypeKind
import logging

# 配置libclang库路径
Config.set_library_file("/usr/lib/llvm-18/lib/libclang.so")

# 初始化日志
logger = logging.getLogger(__name__)

# 存储标准库函数名集合（可根据需要扩展）
STDLIB_FUNCTIONS = {
    'printf', 'scanf', 'fopen', 'fclose', 'malloc', 'free',
    'strcpy', 'strcat', 'memcpy', 'memset', 'exit', 'atoi',
    'itoa', 'strcmp', 'strlen', 'sqrt', 'sin', 'cos'
}

def is_stdlib_function(func_name):
    """判断函数是否为标准库函数"""
    return func_name in STDLIB_FUNCTIONS

def find_decl_ref_in_children(cursor):
    """
    递归查找游标及其子节点中第一个 DECL_REF_EXPR（函数名引用）
    返回：找到的 DECL_REF_EXPR 游标，未找到则返回 None
    """
    # 递归终止条件：当前游标就是目标类型，直接返回
    if cursor.kind == CursorKind.DECL_REF_EXPR:
        return cursor
    # 遍历当前游标的所有子节点，递归查找
    for child in cursor.get_children():
        result = find_decl_ref_in_children(child)
        if result is not None:
            return result
    # 遍历完所有子节点都没找到，返回 None
    return None

def walk_cursor(cursor, path, goal_line, line_symbols):
    """遍历AST，收集指定行的符号引用。
    
    Args:
        cursor: AST 游标
        path: 源文件路径
        goal_line: 目标行号
        line_symbols: 收集的符号字典
    """
    for child in cursor.get_children():
        try:
            # 检查节点是否属于目标文件
            if child.location.file is None or path != child.location.file.name:
                continue

            # 处理目标行上的符号
            line = child.location.line
            if line == goal_line:
                logger.debug(f"Processing node at line {line} - Kind: {child.kind}, Spelling: {child.spelling}")
                
                def collect_symbols(node, call_function=None):
                    """递归收集所有的变量引用和成员访问"""
                    if not node:
                        return
                    
                    # 收集当前节点的符号
                    if node.spelling and node.kind in (CursorKind.DECL_REF_EXPR, CursorKind.MEMBER_REF_EXPR) and node.spelling != call_function:
                        symbol = node.spelling
                        line_symbols[symbol] = node
                        logger.debug(f"Found symbol: {symbol} of kind {node.kind}")
                    
                    # 继续处理子节点
                    for c in node.get_children():
                        collect_symbols(c)

                # 处理各类型的符号
                if child.kind == CursorKind.CALL_EXPR:
                    symbol = child.spelling + '()'
                    line_symbols[symbol] = child
                    logger.debug(f"Found symbol: {symbol} of kind {child.kind}")
                    # 处理函数调用，包括成员函数调用
                    children = list(child.get_children())
                    if children:
                        # 处理函数调用表达式（可能是成员访问链）
                        collect_symbols(children[0], child.spelling)
                        # 处理所有参数
                        for arg in children[1:]:
                            collect_symbols(arg)
                
                elif child.kind in (CursorKind.MEMBER_REF_EXPR, CursorKind.DECL_REF_EXPR):
                    collect_symbols(child)
                else:
                    collect_symbols(child)
                continue  # 已经处理完目标行,不需要再递归
            # 递归处理非目标行的子节点
            walk_cursor(child, path, goal_line, line_symbols)

        except Exception as e:
            # 某些节点的 location/extent 可能引发异常，忽略并继续
            logger.debug(f"Skipping node due to error: {e}")
            pass

    # 不需要返回值
    return

def parse(path, report_line, args=[]):
    """解析代码，获取指定行的符号并返回 (function_name, variable_list)。

    返回值:
        (function_name_or_None, [symbol1, symbol2, ...])

    每个变量只保留最后一次调用，屏蔽库函数。对于不支持的文件或解析失败，返回 (None, []).
    """
    if not path.endswith(('.c', '.cpp', '.h')):
        return None, []

    idx = Index.create()
    line_symbols = {}  # 用字典存储，自动覆盖保留最后一次出现
    try:
        translation_unit = idx.parse(path, args=args, options=0x1)
        walk_cursor(translation_unit.cursor, path, report_line, line_symbols)

        # 处理诊断信息
        for diag in translation_unit.diagnostics:
            if diag.severity > 2:  # 错误级别
                logger.info(f'Error when parse: {diag}')

    except clang.cindex.TranslationUnitLoadError as e:
        logger.info(f'Error parsing translation unit: {path} with args {args}, {e}')
        return None, []

    # 获取包含该行的函数名（若有）
    func_name = get_function(path, report_line, args)

    # 返回 (function_name, 去重后的符号列表（按最后出现顺序）)
    return func_name, list(line_symbols.keys())

def walk_cursor_f(cursor, path, loc_line):
    """查找包含指定行的函数"""
    for node in cursor.get_children():
        if node.location.file is not None and path != node.location.file.name:
            continue
        if node.kind == CursorKind.FUNCTION_DECL:
            start_line = node.extent.start.line
            end_line = node.extent.end.line
            if start_line <= loc_line <= end_line:
                return node.spelling
        # 递归查找
        result = walk_cursor_f(node, path, loc_line)
        if result:
            return result
    return None

def get_function(path, loc_line, args=[]):
    """获取包含指定行的函数名"""
    idx = Index.create()
    try:
        translation_unit = idx.parse(path, args=args, options=0x1)
        return walk_cursor_f(translation_unit.cursor, path, loc_line)
    except Exception as e:
        logger.info(f'Error in get_function: {e}')
        return None

# 测试代码
if __name__ == "__main__":
    test_path = '/home/nishikino/new_version_benchmarks/output/redis-8.2.2/report-ebqQJ41/connection.h'
    test_line = 7
    args = ['-std=c++11']
    result = parse(test_path, test_line, args)
    print(f"第{test_line}行的符号（去重并保留最后一次调用，排除库函数）:")
    print(result)
    
    # 测试获取包含指定行的函数
    func_name = get_function(test_path, test_line, args)
    print(f"包含第{test_line}行的函数: {func_name}")