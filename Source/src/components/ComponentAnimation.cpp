#include "core/hepch.h"

#include "Application.h"

#include "modules/ModuleResources.h"

#include "ComponentAnimation.h"
#include "ComponentTransform.h"

#include "resources/ResourceAnimation.h"
#include "animation/AnimationController.h"

#include "ui/WindowStateMachine.h"
#include "utils/ComponentUtility.h"

Hachiko::ComponentAnimation::ComponentAnimation(GameObject* container) :
    Component(Type::ANIMATION, container)
{
    auxiliary_name = game_object->name;
    controller = new AnimationController();
    windowStateMachine = new WindowStateMachine(game_object->name);
}

Hachiko::ComponentAnimation::~ComponentAnimation()
{
    delete windowStateMachine;
    App->resources->ReleaseResource(state_machine);
    delete controller;
}

void Hachiko::ComponentAnimation::StartAnimating()
{
    if (!state_machine)
    {
        return;
    }

    PlayNode(state_machine->GetDefaultNode(), 0);
}

void Hachiko::ComponentAnimation::StopAnimating()
{
    controller->Stop();

    active_node = 0;
}

void Hachiko::ComponentAnimation::ResetState()
{
    if (!state_machine)
    {
        return;
    }

    controller->Stop();
    PlayNode(state_machine->GetDefaultNode(), 0);
}

void Hachiko::ComponentAnimation::SetTimeScaleMode(TimeScaleMode new_time_scale_mode)
{
    Component::SetTimeScaleMode(new_time_scale_mode);
    controller->SetTimeScaleMode(new_time_scale_mode);
}

void Hachiko::ComponentAnimation::SendTrigger(const std::string& trigger)
{
    if (!state_machine)
    {
        return;
    }

    std::string active = GetActiveNode();

    unsigned int transition_in_any = state_machine->GetNumTransitions();

    for (unsigned int i = 0; i < state_machine->GetNumTransitions(); ++i)
    {
        // We go to the next transition if it does not have the correct trigger
        if (state_machine->GetTransitionTrigger(i) != trigger)
        {
            continue;
        }
        // First we check if the current node has a direct transition
        if (state_machine->GetTransitionSource(i) == active)
        {
            PlayNode(state_machine->GetTransitionTarget(i), state_machine->GetTransitionBlend(i));
            return;
        }
        // If not we check if nodes starting with "ANY" have a transition with said trigger
        if (state_machine->GetTransitionSource(i).rfind(state_machine->any_state_node_name, 0) != std::string::npos)
        {
            transition_in_any = i;
        }
    }

    // If we've found a transition in "ANY", complete it after all other transitions have been checked
    if (transition_in_any < state_machine->GetNumTransitions())
    {
        PlayNode(state_machine->GetTransitionTarget(transition_in_any), state_machine->GetTransitionBlend(transition_in_any));
    }
}

std::string Hachiko::ComponentAnimation::GetActiveNode() const
{
    if (state_machine != nullptr && active_node < state_machine->GetNumNodes())
    {
        return state_machine->GetNodeName(active_node);
    }

    return {};
}

void Hachiko::ComponentAnimation::Update()
{
    controller->Update(EngineTimer::delta_time * 1000 * speed, reverse);

    if (game_object != nullptr)
    {
        UpdatedGameObject(game_object);
    }
}

void Hachiko::ComponentAnimation::UpdatedGameObject(GameObject* go)
{
    float3 position;
    Quat rotation;

    if (controller->GetTransform(controller->GetCurrentInstance(), go->name.c_str(), position, rotation))
    {
        go->GetTransform()->SetLocalPosition(position);
        go->GetTransform()->SetLocalRotation(rotation);
    }

    for (GameObject* child : go->children)
    {
        UpdatedGameObject(child);
    }
}

void Hachiko::ComponentAnimation::PlayNode(const std::string& node, unsigned int blend)
{
    if (!state_machine)
    {
        return;
    }

    PlayNode(state_machine->FindNode(node), blend);
}

void Hachiko::ComponentAnimation::PlayNode(unsigned int node_idx, unsigned int blend)
{
    if (node_idx < state_machine->GetNumNodes())
    {
        active_node = node_idx;
        unsigned int clip_idx = state_machine->FindClip(state_machine->GetNodeClip(node_idx));

        if (clip_idx < state_machine->GetNumClips())
        {
            ResourceAnimation* current_animation = state_machine->GetClipRes(clip_idx);

            if (current_animation != 0)
            {
                controller->Play(current_animation, state_machine->GetClipLoop(clip_idx), blend);
            }
        }
    }
}

void Hachiko::ComponentAnimation::DrawGui()
{
    ImGui::PushID(this);

    if (ImGuiUtils::CollapsingHeader(this, "Animation"))
    {
        Widgets::Checkbox("Reverse", &reverse);

        /* LOAD STATE MACHINE */

        LoadStateMachine();

        /* CREATE NEW STATE MACHINE */

        if (ImGui::Button("New state machine", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        {
            ImGui::OpenPopup("NewStateMachine");
        }

        if (ImGui::BeginPopup("NewStateMachine"))
        {
            ImGui::InputText("Name", &auxiliary_name, ImGuiInputTextFlags_EnterReturnsTrue);
            if (ImGui::Button("Create state machine", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            {
                App->resources->ReleaseResource(state_machine);
                std::vector<UID> uids = App->resources->CreateAsset(Resource::Type::STATE_MACHINE, auxiliary_name);
                state_machine = dynamic_cast<ResourceStateMachine*>(App->resources->GetResource(Resource::Type::STATE_MACHINE, uids[0]));
                // Once a new state machine is added a "none" clip is added to it
                state_machine->AddClip(Hachiko::ResourceStateMachine::none_clip_name, 0, false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (state_machine != nullptr)
        {
            windowStateMachine->SetStateMachine(state_machine);
            windowStateMachine->Update();

            if (Widgets::Input("Resource name", state_machine->state_m_name))
            {
                // state_machine->state_m_name = name;
                state_machine->Save();
            }

            if (ImGui::Button("Add clip", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            {
                state_machine->AddClip("noname", 0, true);
                state_machine->Save();
            }

            if (state_machine->GetNumClips() > 0)
            {
                ImGui::Separator();
            }

            unsigned int clip_idx = 0;
            while (clip_idx < state_machine->GetNumClips())
            {
                const ResourceAnimation* res = state_machine->GetClipRes(clip_idx);

                std::string clip = state_machine->GetClipName(clip_idx);
                // Don't show clip "none"
                if (strcmp(clip.c_str(), ResourceStateMachine::none_clip_name) == 0)
                {
                    ++clip_idx;
                    continue;
                }

                ImGui::PushID(clip_idx);
                
                if (Widgets::Input("Clip name", clip))
                {
                    state_machine->SetClipName(clip_idx, clip);
                    state_machine->Save();
                }

                Widgets::Label("Resource", res ? res->GetName() : "Unknown");

                AnimationSelector(clip_idx);

                bool loop = state_machine->GetClipLoop(clip_idx);
                if (Widgets::Checkbox("Loop", &loop))
                {
                    state_machine->SetClipLoop(clip_idx, loop);
                    state_machine->Save();
                }

                if (ImGui::Button("Remove", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                {
                    state_machine->RemoveClip(clip_idx);
                    state_machine->Save();
                }
                else
                {
                    ++clip_idx;
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            if (state_machine->GetNumClips() > 0)
            {
                if (ImGui::CollapsingHeader("Animation controller", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::Button(StringUtils::Concat(ICON_FA_PLAY, "Play animation").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        StartAnimating();
                    }

                    if (ImGui::Button(StringUtils::Concat(ICON_FA_STOP, "Stop animation").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        StopAnimating();
                    }

                    if (ImGui::Button(StringUtils::Concat(ICON_FA_RETWEET, "Reset state").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        ResetState();
                    }

                    if (state_machine->GetNumTransitions() > 0)
                    {
                        ImGui::Separator();
                        std::string active_node = GetActiveNode();

                        for (unsigned int i = 0; i < state_machine->GetNumTransitions(); ++i)
                        {
                            if (state_machine->GetTransitionSource(i) == active_node)
                            {
                                std::string trigger = state_machine->GetTransitionTrigger(i);
                                if (!trigger.empty() && ImGui::Button(trigger.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                                {
                                    ImGui::Separator();
                                    SendTrigger(trigger);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ImGui::PopID();
}

void Hachiko::ComponentAnimation::LoadStateMachine()
{
    const std::string title = StringUtils::Concat("State machine selector#", std::to_string(uid));
    if (ImGui::Button("Select State machine", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
    {
        ImGuiFileDialog::Instance()->OpenDialog(title.c_str(),
                                                "Select state machine",
                                                ".stm",
                                                "./assets/state_machine/",
                                                1,
                                                nullptr,
                                                ImGuiFileDialogFlags_DisableCreateDirectoryButton | ImGuiFileDialogFlags_HideColumnType | ImGuiFileDialogFlags_HideColumnDate);
    }

    if (ImGuiFileDialog::Instance()->Display(title.c_str()))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string meta_path = StringUtils::Concat(ImGuiFileDialog::Instance()->GetCurrentFileName(), META_EXTENSION);
            YAML::Node meta = YAML::LoadFile("./assets/state_machine/" + meta_path);

            // State machine asset will only have a state machine resource
            const UID res_uid = meta[RESOURCES][0][RESOURCE_ID].as<UID>();
            App->resources->ReleaseResource(state_machine);
            state_machine = dynamic_cast<ResourceStateMachine*>(App->resources->GetResource(Resource::Type::STATE_MACHINE, res_uid));
        }

        ImGuiFileDialog::Instance()->Close();
    }
}

void Hachiko::ComponentAnimation::AnimationSelector(unsigned clip_idx)
{
    const std::string title = StringUtils::Concat("Select model#", std::to_string(uid));
    if (ImGui::Button(ICON_FA_UPLOAD, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
    {
        editing_clip_idx = clip_idx;
        ImGuiFileDialog::Instance()->OpenDialog(title.c_str(),
                                                "Select clips source",
                                                MODEL_EXTENSION,
                                                ASSETS_FOLDER_MODEL,
                                                1,
                                                nullptr,
                                                ImGuiFileDialogFlags_DisableCreateDirectoryButton | ImGuiFileDialogFlags_HideColumnType | ImGuiFileDialogFlags_HideColumnDate);
    }
    ImGuiUtils::DisplayTooltip("Select clip source");

    if (clip_idx != editing_clip_idx)
    {
        return;
    }

    if (ImGuiFileDialog::Instance()->Display(title.c_str()))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string meta_path = ImGuiFileDialog::Instance()->GetFilePathName();
            meta_path.append(META_EXTENSION);
            selected_model_meta = YAML::LoadFile(meta_path);

            selected_asset_clip_names.clear();

            for (unsigned i = 0; i < selected_model_meta[ANIMATIONS].size(); ++i)
            {
                selected_asset_clip_names.push_back(selected_model_meta[ANIMATION_NAMES][i].as<std::string>());
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    constexpr const char* popup_name = "Clip selector";
    if (!selected_asset_clip_names.empty())
    {
        ImGui::OpenPopup(popup_name);
    }

    if (ImGui::BeginPopup(popup_name))
    {
        for (unsigned i = 0; i < selected_asset_clip_names.size(); ++i)
        {
            if (ImGui::MenuItem(selected_asset_clip_names[i].c_str()))
            {
                const UID selected_animation_id = selected_model_meta[ANIMATIONS][i].as<UID>();

                state_machine->SetClipRes(clip_idx, selected_animation_id);
                state_machine->Save();
                selected_asset_clip_names.clear();
            }
        }
        ImGui::EndPopup();
    }
}

void Hachiko::ComponentAnimation::Save(YAML::Node& node) const
{
    node.SetTag("animation");
    if (state_machine == nullptr)
    {
        node[M_STATE_MACHINE] = 0;
        return;
    }
    node[M_STATE_MACHINE] = state_machine->GetID();
}

void Hachiko::ComponentAnimation::Load(const YAML::Node& node)
{
    if (node[M_STATE_MACHINE].IsDefined())
    {
        const UID state_machine_uid = node[M_STATE_MACHINE].as<UID>();
        App->resources->ReleaseResource(state_machine);
        state_machine = dynamic_cast<ResourceStateMachine*>(App->resources->GetResource(Resource::Type::STATE_MACHINE, state_machine_uid));
    }
}

void Hachiko::ComponentAnimation::CollectResources(const YAML::Node& node, std::map<Resource::Type, std::set<UID>>& resources)
{
    ComponentUtility::CollectResource(
        Resource::Type::STATE_MACHINE,
        node[M_STATE_MACHINE],
        resources);
}