import sys
import os
import subprocess

parent_dir = os.path.dirname(os.getcwd())
shaders_dir = os.path.join(parent_dir, 'shaders')
shader_model = "5_1"

def compile_shader(relative_path: str) -> None:
	name =  relative_path.replace(f"{os.path.dirname(relative_path)}{os.path.sep}", '')

	src_path = os.path.join(shaders_dir, relative_path)
	bin_path = os.path.join(os.path.join(shaders_dir, 'bin'), name)
	bin_path = bin_path.split('.')[0]
	shader_name = name.split('.')[0]
	if shader_name.endswith('ps'):
		shader_type = 'ps'
	elif shader_name.endswith('vs'):
		shader_type = 'vs'
	else:
		return

	compile_command = f'fxc.exe "{src_path}" /nologo /E "main" /T {shader_type}_{shader_model} /Zi /Fo "{bin_path}.cso" /Fd "{bin_path}.pdb"'
	try:
		result = subprocess.check_output(compile_command, shell=True).decode()
	except Exception as e:
		print(f"Exception with file {name}")


def compile_shaders(dirname):
	if not os.path.exists(os.path.join(shaders_dir, 'bin')):
		os.mkdir(os.path.join(shaders_dir, "bin"))
	abs_path = os.path.join(shaders_dir, dirname)
	files = os.listdir(abs_path)
	for file in files:
		file = os.path.join(dirname, file)
		if os.path.isdir(os.path.join(shaders_dir, file)):
			compile_shaders(file)
		else:
			compile_shader(file)

if __name__ == "__main__":
	compile_shaders('')