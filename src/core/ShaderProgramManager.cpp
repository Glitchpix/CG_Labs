#include "ShaderProgramManager.hpp"

#include "config.hpp"

#include "Log.h"
#include "opengl.hpp"
#include "various.hpp"

#include <imgui.h>

#include <type_traits>
#include <exception>

ShaderProgramManager::~ShaderProgramManager()
{
	for (auto& i : program_entries) {
		if (i.first != 0u) {
			glDeleteProgram(*i.first);
			i.first = 0u;
		}
	}
}

GLuint& ShaderProgramManager::CreateAndRegisterProgram(char const* const program_name, ProgramData const& program_data)
{
	if (!GLAD_GL_ARB_compute_shader) {
		for (auto const& i : program_data) {
			if (i.first == ShaderType::compute) {
				LogError("Compute shaders aren't exposed on your computer (needed for shader '%s'.", i.second.c_str());
				throw std::runtime_error("Unsupported operation!");
			}
		}
	}

	program_entries.emplace_back(std::make_unique<GLuint>(0u), program_data);
	program_names.emplace_back(program_name);

	const auto programIndex = program_entries.size() - 1;
	ProcessProgram(programIndex);
	return *(program_entries[programIndex].first);
}

GLuint& ShaderProgramManager::CreateAndRegisterComputeProgram(char const* const program_name, std::string const& filename)
{
	if (!GLAD_GL_ARB_compute_shader) {
		LogError("Compute shaders aren't exposed on your computer (needed for shader '%s'.", filename.c_str());
		throw std::runtime_error("Unsupported operation!");
	}

	program_entries.emplace_back(std::make_unique<GLuint>(0u), ProgramData{ { ShaderType::compute, filename } });
	program_names.emplace_back(program_name);

	const auto programIndex = program_entries.size() - 1;
	ProcessProgram(programIndex);

	return *(program_entries[programIndex].first);
}

bool ShaderProgramManager::ReloadAllPrograms()
{
	bool encountered_failures = false;
	for (std::size_t i = 0; i < program_entries.size(); ++i) {
		auto& program = program_entries[i].first;
		if (program != 0u)
			glDeleteProgram(*program);
		program = 0u;
		ProcessProgram(i);
		encountered_failures |= program == 0u;
	}

	return !encountered_failures;
}

ShaderProgramManager::SelectedProgram ShaderProgramManager::SelectProgram(std::string const& label, std::int32_t& program_index)
{
	SelectedProgram selection_result;
	if (program_index >= program_entries.size()) {
		LogError("Invalid program index '%d': only %d programs are registered.", program_index, program_entries.size());
		return selection_result;
	}

	selection_result.was_selection_changed = ImGui::Combo(label.c_str(), &program_index, program_names.data(), static_cast<int>(program_names.size()));
	selection_result.program = program_entries.at(program_index).first.get();
	selection_result.name = program_names.at(program_index);
	return selection_result;
}

void ShaderProgramManager::ProcessProgram(std::size_t const program_index)
{
	auto& program_entry = program_entries[program_index];
	auto& program = program_entry.first;
	auto const& program_data = program_entry.second;

	std::vector<GLuint> shaders;
	shaders.reserve(program_data.size());

	for (auto const& i : program_data) {
		std::string const full_filename = config::shaders_path(i.second);
		auto const shader_source = utils::slurp_file(full_filename);
		if (shader_source.empty()) {
			LogError("Retrieval of shader '%s' failed; see previous message for details.", full_filename.c_str());
			return;
		}

		GLuint shader = utils::opengl::shader::generate_shader(static_cast<std::underlying_type<ShaderType>::type>(i.first), shader_source);
		if (shader == 0u) {
			for (auto& shader : shaders)
				glDeleteShader(shader);
			LogError("Compilation of shader '%s' failed; see previous message for details.", full_filename.c_str());
			return;
		}
		shaders.push_back(shader);
	}

	*program = utils::opengl::shader::generate_program(shaders);
	utils::opengl::debug::nameObject(GL_PROGRAM, *program, program_names[program_index]);

	for (auto& shader : shaders)
		glDeleteShader(shader);
}
