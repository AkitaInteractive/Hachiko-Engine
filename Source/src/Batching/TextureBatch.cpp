#include "core/hepch.h"
#include "TextureBatch.h"

#include "Batching/BatchManager.h"
#include "components/ComponentMeshRenderer.h"
#include "resources/ResourceMaterial.h"

#include "modules/ModuleProgram.h"
#include "modules/ModuleTexture.h"
#include "core/rendering/Program.h"

Hachiko::TextureBatch::~TextureBatch()
{
    for (auto& resource : resources)
    {
        delete resource.second;
    }
    resources.clear();

    for (TextureArray* textureArray : texture_arrays)
    {
        glDeleteTextures(1, &textureArray->id);
        delete textureArray;
    }
    texture_arrays.clear();

    glDeleteBuffers(1, &material_buffer);
}

void Hachiko::TextureBatch::AddMaterial(const ResourceMaterial* resource_material)
{
    if (!resource_material)
    {
        return;
    }

    if (resource_material->HasDiffuse())
    {
        AddTexture(resource_material->diffuse);
    }
    if (resource_material->HasSpecular())
    {
        AddTexture(resource_material->specular);
    }
    if (resource_material->HasNormal())
    {
        AddTexture(resource_material->normal);
    }
    if (resource_material->HasMetalness())
    {
        AddTexture(resource_material->metalness);
    }
    if (resource_material->HasEmissive())
    {
        AddTexture(resource_material->emissive);
    }
}

void Hachiko::TextureBatch::AddTexture(const ResourceTexture* texture)
{
    auto it = resources.find(texture);

    if (it == resources.end())
    {
        // It will be formatted when generating batches
        resources[texture] = new TexAddress();

        // Search for the its textureArray
        bool textureArrayFound = false;
        for (TextureArray* textureArray : texture_arrays)
        {
            if (EqualLayout(*textureArray, *texture))
            {
                textureArrayFound = true;
                textureArray->depth += 1;
                break;
            }
        }

        // If there is none, create it
        if (!textureArrayFound)
        {
            TextureArray* textureArray = new TextureArray();
            textureArray->depth = 1;
            textureArray->width = texture->width;
            textureArray->height = texture->height;
            textureArray->format = texture->format;
            textureArray->wrap_mode = texture->wrap;
            textureArray->min_filter = texture->min_filter;
            textureArray->mag_filter = texture->mag_filter;

            texture_arrays.push_back(textureArray);
        }

        // Set the flag as not all textures are loaded
        loaded = false;
    }
}

void Hachiko::TextureBatch::BuildBatch(unsigned component_count)
{
    // TODO: improve performance (for inside for)

    int maxLayers; // maximum number of array texture layers
    int maxUnits; // maximum number of texture units
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);

    // Security check, amount of texture units
    if (texture_arrays.size() > maxUnits - 8)
    {
        HE_ERROR("There are more texture units than the maximum.");
    }

    unsigned index = 0;
    for (unsigned i = 0; i < texture_arrays.size(); ++i)
    {
        // Security check, amount of layers in a texture array
        if (texture_arrays[i]->depth > maxLayers)
        {
            HE_ERROR("There are layers in a texture array than the maximum.");
        }

        glGenTextures(1, &texture_arrays[i]->id);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture_arrays[i]->id);

        // Generate texture array
        unsigned sizedFormat = GL_RGBA8;
        if (texture_arrays[i]->format == GL_RGBA)
        {
            sizedFormat = GL_RGBA8;
        }
        else if (texture_arrays[i]->format == GL_RGB)
        {
            sizedFormat = GL_RGB8;
        }

        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 3, sizedFormat, texture_arrays[i]->width, texture_arrays[i]->height, texture_arrays[i]->depth);
        unsigned depth = 0;
        for (auto& resource : resources)
        {
            if (EqualLayout(*texture_arrays[i], *resource.first))
            {
                resource.second->texIndex = i;
                resource.second->layerIndex = depth;

                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, // target
                                0, // level
                                0, // xoffset
                                0, // yoffset
                                depth, // zoffset
                                texture_arrays[i]->width, // width
                                texture_arrays[i]->height, // height
                                1, // depth
                                texture_arrays[i]->format, // format
                                GL_UNSIGNED_BYTE, // type
                                resource.first->data // texture_data
                );

                ++depth;
            }
        }

        // Array texture parameters
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 2);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, texture_arrays[i]->wrap_mode);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, texture_arrays[i]->wrap_mode);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, texture_arrays[i]->min_filter);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, texture_arrays[i]->mag_filter);
       
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }

    //glDeleteBuffers(1, &material_buffer);
    material_buffer_data = static_cast<Material*>(
        App->program->CreatePersistentBuffers(material_buffer, static_cast<int>(ModuleProgram::BINDING::MATERIAL), BatchingProperties::MAX_SEGMENTS * component_count * sizeof(Material)));

    loaded = true;
}

void Hachiko::TextureBatch::UpdateBatch(int segment, const std::vector<const ComponentMeshRenderer*>& components, unsigned component_count)
{
    // Update material data
    materials.clear();
    materials.resize(components.size());

    size_t offset = component_count * segment;
    
    for (unsigned i = 0; i < components.size(); ++i)
    {
        const ResourceMaterial* material = components[i]->GetResourceMaterial();

        if (!material)
        {
            continue;
        }

        materials[i].diffuse_color = material->diffuse_color;
        materials[i].specular_color = material->specular_color;
        materials[i].emissive_color = material->emissive_color;
        materials[i].tint_color = components[i]->GetTintColor();
        materials[i].diffuse_flag = material->HasDiffuse();
        materials[i].specular_flag = material->HasSpecular();
        materials[i].normal_flag = material->HasNormal();
        materials[i].metallic_flag = material->HasMetalness();
        materials[i].emissive_flag = material->HasEmissive();

        if (materials[i].diffuse_flag)
        {
            materials[i].diffuse_map = (*resources[material->diffuse]);
        }
        if (materials[i].specular_flag)
        {
            materials[i].specular_map = (*resources[material->specular]);
        }
        if (materials[i].normal_flag)
        {
            materials[i].normal_map = (*resources[material->normal]);
        }
        if (materials[i].metallic_flag)
        {
            materials[i].metallic_map = (*resources[material->metalness]);
        }
        if (materials[i].emissive_flag)
        {
            materials[i].emissive_map = (*resources[material->emissive]);
        }

        materials[i].smoothness = material->smoothness;
        materials[i].metalness_value = material->metalness_value;
        materials[i].is_metallic = material->is_metallic;
        materials[i].is_transparent = material->is_transparent;

        if (components[i]->OverrideMaterialActive())
        {
            materials[i].emissive_flag = components[i]->GetOverrideMaterialFlag() ? 1 : 0;
            materials[i].emissive_color = components[i]->GetOverrideEmissiveColor();
        }

        materials[i].dissolve_progress = components[i]->GetDissolveProgress();

        // Copy material to persistent buffer:
        material_buffer_data[offset + i] = materials[i];
    }
}

void Hachiko::TextureBatch::BindBatch(int segment, const Program* program, unsigned component_count)
{
    // Bind textures
    const std::vector<int> texture_slots = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
    program->BindUniformInts("allMyTextures", texture_arrays.size(), &texture_slots[0]);

    for (unsigned i = 0; i < texture_arrays.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE11 + i);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture_arrays[i]->id);
    }

    // Bind persistent buffers
    glBindBuffer(GL_ARRAY_BUFFER, material_buffer);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                      static_cast<int>(ModuleProgram::BINDING::MATERIAL),
                      material_buffer,
                      (component_count * sizeof(Material)) * segment,
                      component_count * sizeof(Material));
}

void Hachiko::TextureBatch::ImGuiWindow()
{
    ImGui::TextWrapped("- Amount of texture arrays: %i", texture_arrays.size());

    if (ImGui::TreeNodeEx(&texture_arrays, ImGuiTreeNodeFlags_None, "Texture arrays"))
    {
        for (unsigned i = 0; i < texture_arrays.size(); ++i)
        {
            ImGui::TextWrapped("TextureArray %i", i);
            ImGui::TextWrapped("- Amount of texture resources: %i", resources.size());

            ImGui::TextWrapped("- Depth (number of textures): %i", texture_arrays[i]->depth);
            ImGui::TextWrapped("- Width: %i", texture_arrays[i]->width);
            ImGui::TextWrapped("- Height: %i", texture_arrays[i]->height);
            ImGui::TextWrapped("- Format: %i", texture_arrays[i]->format);
            ImGui::TextWrapped("- Wrap Mode: %i", texture_arrays[i]->wrap_mode);
            ImGui::TextWrapped("- Min Filter: %i", texture_arrays[i]->min_filter);
            ImGui::TextWrapped("- Mag Filter: %i", texture_arrays[i]->mag_filter);

            if (ImGui::TreeNodeEx(&resources, ImGuiTreeNodeFlags_None, "Textures"))
            {
                for (auto& resource : resources)
                {
                    if (EqualLayout(*texture_arrays[i], *resource.first))
                    {
                        ImGui::Text("%llu ", resource.first->GetID());
                        ImGui::SameLine();
                        ImGui::Text(resource.first->path.c_str());
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

bool Hachiko::TextureBatch::EqualLayout(const TextureArray& texture_layout, const ResourceTexture& texture)
{
    return (
        texture_layout.width == texture.width && 
        texture_layout.height == texture.height && 
        texture_layout.format == texture.format && 
        texture_layout.wrap_mode == texture.wrap && 
        texture_layout.min_filter == texture.min_filter &&
        texture_layout.mag_filter == texture.mag_filter
    );
}
