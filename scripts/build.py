import shutil
import sys
import os
import subprocess

parent_dir = os.path.dirname(os.getcwd())
shaders_dir = os.path.join(parent_dir, 'shaders')
shader_model = "6_5"

def compile_shader(relative_path: str) -> None:
	if relative_path.endswith('hlsli'):
		return
	name =  relative_path.replace(f"{os.path.dirname(relative_path)}{os.path.sep}", '')

	src_path = os.path.join(shaders_dir, relative_path)
	bin_path = os.path.join(os.path.join(shaders_dir, 'bin'), name)
	bin_path = bin_path.split('.')[0]

	override_model = shader_model
	if name.endswith('_ps.hlsl'):
		shader_type = 'ps'
	elif name.endswith('_vs.hlsl'):
		shader_type = 'vs'
	elif name.endswith('_cs.hlsl'):
		shader_type = 'cs'
	elif name.endswith('_hs.hlsl'):
		shader_type = 'hs'
	elif name.endswith('_ds.hlsl'):
		shader_type = 'ds'
	elif name.endswith('_gs.hlsl'):
		shader_type = 'gs'
	elif name.endswith('_ms.hlsl'):
		shader_type = 'ms'
		override_model = '6_5'
	elif name.endswith('_as.hlsl'):
		shader_type = 'as'
		override_model = '6_5'
	elif name.endswith('rts.hlsl'):
		shutil.copy(src_path, f"{bin_path}.hlsl")
		return
	else:
		return

	compile_command = f'dxc.exe /nologo /E "main" /T {shader_type}_{override_model} /Fo "{bin_path}.cso" /Fd "{bin_path}.pdb" /Od /Zi /all_resources_bound /Qembed_debug "{src_path}"'
	try:
		result = subprocess.check_output(compile_command, shell=True).decode()
	except Exception as e:
		print(f"Exception with file {name}")


def compile_shaders(dirname):
	abs_path = os.path.join(shaders_dir, dirname)
	files = os.listdir(abs_path)
	for file in files:
		file = os.path.join(dirname, file)
		if os.path.isdir(os.path.join(shaders_dir, file)):
			compile_shaders(file)
		else:
			compile_shader(file)

if __name__ == "__main__":
	if os.path.exists(os.path.join(shaders_dir, "bin")):
		shutil.rmtree(os.path.join(shaders_dir, "bin"))
	os.mkdir(os.path.join(shaders_dir, "bin"))
	compile_shaders('')