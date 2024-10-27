#include "egpch.h"
#include "PhysicsActor.h"
#include "PhysicsRagdollActor.h"
#include "PhysXInternal.h"
#include "PhysicsUtils.h"

#include "Eagle/Components/Components.h"
#include "Eagle/Animation/AnimationSystem.h"

namespace Eagle
{
    // Just an example code, not used. Creates a body for each bone.
#if 0
    static void CreateArticulationChain(const BoneNode& node, const SkeletalPose& currentPose, const BonesMap& boneMap, const glm::mat4& baseTransform, physx::PxScene* scene,
        PhysicsRagdollActor::BoneData& physicsBoneData, PhysicsActorPayload& payload, const PhysicsSettings& settings, const glm::mat4& compWorldTr, const glm::mat4& baseBoneTransform = glm::mat4(1.f), physx::PxRigidDynamic* parentBody = nullptr)
    {
        using namespace physx;

        auto& physics = PhysXInternal::GetPhysics();
        const auto it = currentPose.Bones.find(node.Name);
        glm::mat4 localParentTr = (it != currentPose.Bones.end() ? Math::ToTransformMatrix(it->second) : node.Transformation);
        glm::mat4 tr = baseTransform * localParentTr;
        glm::mat4 boneTr = baseBoneTransform * localParentTr; // We don't want to include components world transformation into bone tr since it's already applied during the rendering

        auto itParentBoneMap = boneMap.find(node.Name);
        if (itParentBoneMap == boneMap.end())
        {
            for (const auto& child : node.Children)
            {
                if (child.bVirtualBone)
                    continue;

                CreateArticulationChain(child, currentPose, boneMap, tr, scene, physicsBoneData, payload, settings, compWorldTr, boneTr, parentBody);
            }
            return;
        }

        const PxTransform transform = PhysXUtils::ToPhysXTranform(tr);
        const glm::vec3 parentPos = PhysXUtils::FromPhysXVector(transform.p);
        const PxTransform jointTransform{ transform.p, PhysXUtils::ToPhysXQuat(glm::quat_cast(itParentBoneMap->second.Offset)) };

        for (const auto& child : node.Children)
        {
            constexpr float minRadius = 0.005f;
            constexpr float maxRadius = 0.05f;
            if (child.bVirtualBone)
                continue;

            const auto itChild = currentPose.Bones.find(child.Name);
            const glm::mat4 childLocalTr = (itChild != currentPose.Bones.end() ? Math::ToTransformMatrix(itChild->second) : child.Transformation);
            const glm::mat4 childWorldTr = tr * childLocalTr;
            const glm::vec3 childPos = Math::DecomposeTransformMatrix(childWorldTr).Location;

            const float len = glm::length(parentPos - childPos) * 0.9f; // shorten to reduce overlap
            const float radius = glm::clamp(len * 0.1f, minRadius, maxRadius);
            const float len_minus_2r = len - 2.0f * radius;
            const float half_height = len_minus_2r * 0.5f;
            const glm::vec3 bodyLocation = (parentPos + childPos) * 0.5f;

            const glm::mat4 lookAt = glm::inverse(glm::lookAt(bodyLocation, childPos, glm::vec3(0, 1, 0)));
            const PxQuat q = PhysXUtils::ToPhysXQuat(Math::DecomposeTransformMatrix(lookAt).Rotation.GetQuat());

            PxRigidDynamic* body = physics.createRigidDynamic(PxTransform(PhysXUtils::ToPhysXVector(bodyLocation), q));
            body->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
            body->userData = &payload;
            scene->addActor(*body);
            if (!parentBody)
            {
                // Create a parent to hold together all other bodies
                parentBody = PxCloneDynamic(physics, body->getGlobalPose(), *body);
                scene->addActor(*parentBody);
                physicsBoneData.Body = parentBody;
                physicsBoneData.Name = node.Name;
                physicsBoneData.BoneWorldTr = boneTr;
                physicsBoneData.OriginalBodyTrInv = glm::inverse(Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(parentBody->getGlobalPose())));
                parentBody->userData = &payload;
                parentBody->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
            }

            // Setup collider
            const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), PxHalfPi, glm::vec3(0.0f, 1.0f, 0.0f));
            PxShape* shape = physics.createShape(PxCapsuleGeometry(radius, half_height), *material);
            shape->setSimulationFilterData(s_FilterData);
            PxTransform local(PhysXUtils::ToPhysXQuat(glm::quat_cast(rot)));
            shape->setLocalPose(local);
            body->attachShape(*shape);

            // Setup Joint
            PxD6Joint* joint = PxD6JointCreate(physics,
                parentBody,
                parentBody->getGlobalPose().transformInv(jointTransform),
                body,
                body->getGlobalPose().transformInv(jointTransform));

            constexpr float twist = 22.5f;
            constexpr float swing = 45.f;
            joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);
            joint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
            joint->setSwingLimit(physx::PxJointLimitCone(glm::radians(swing), glm::radians(swing)));
            joint->setTwistLimit(physx::PxJointAngularLimitPair(-glm::radians(twist), glm::radians(twist)));

            auto& childData = physicsBoneData.Children.emplace_back();
            childData.Body = body;
            childData.Joint = joint;
            childData.Name = child.Name;
            childData.BoneWorldTr = boneTr * childLocalTr;
            childData.OriginalBodyTrInv = glm::inverse(compWorldTr * Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(body->getGlobalPose())));

            CreateArticulationChain(child, currentPose, boneMap, tr, scene, childData, payload, settings, compWorldTr, boneTr, body);
        }
    }
#endif

    static void CreateArticulationChain(const SkeletalRagdollBones& merged, const SkeletalPose& currentPose, const BonesMap& boneMap, physx::PxScene* scene, PhysicsRagdollActor::BoneData& physicsBoneData, PhysicsActorPayload& payload,
        const PhysicsSettings& settings, const glm::mat4& worldTransform, const glm::mat4& compWorldTrInv, const physx::PxMaterial& material, float twist, float swing, const physx::PxVec3& linearVelocity,
        const physx::PxVec3& angularVelocity, physx::PxRigidDynamic* parentBody = nullptr)
    {
        using namespace physx;

        // TODO: Expose
        static physx::PxFilterData s_FilterData(1, 2, 0, 0);
        auto& physics = PhysXInternal::GetPhysics();
        const auto it = currentPose.Bones.find(merged.Name);

        auto itParentBoneMap = boneMap.find(merged.Name);
        if (itParentBoneMap == boneMap.end())
        {
            for (const auto& child : merged.Children)
            {
                CreateArticulationChain(child, currentPose, boneMap, scene, physicsBoneData, payload, settings, worldTransform, compWorldTrInv, material, twist, swing, linearVelocity, angularVelocity, parentBody);
            }
            return;
        }

        const glm::mat4 boneWorldTransform = worldTransform * merged.LocalTransform;
        const PxTransform transform = PhysXUtils::ToPhysXTranform(boneWorldTransform);

        const glm::vec3 locationOffset = merged.UserOffset.Location;
        glm::vec3 parentPos = PhysXUtils::FromPhysXVector(transform.p) + locationOffset;
        const PxTransform jointTransform{ transform.p, PhysXUtils::ToPhysXQuat(glm::quat_cast(itParentBoneMap->second.Offset)) };

        {
            constexpr float minRadius = 0.005f;
            constexpr float maxRadius = 0.05f;

            const glm::vec3 aabbWorld = glm::vec3(worldTransform * glm::vec4(merged.AABB.Center(), 1.f)) + locationOffset;
            glm::vec3 childPos = parentPos + (aabbWorld - parentPos) * 2.f;

            const float len = glm::length(parentPos - childPos) * 0.95f; // shorten to reduce overlap
            const float radius = glm::clamp(len * 0.1f, minRadius, maxRadius);
            const float lenMinus2r = len - 2.0f * radius;
            const float halfHeight = lenMinus2r * 0.5f;
            const glm::vec3 bodyLocation = (parentPos + childPos) * 0.5f;

            // Rotate around `bodyLocation`
            parentPos = bodyLocation + glm::rotate(merged.UserOffset.Rotation.GetQuat(), parentPos - bodyLocation);
            childPos = bodyLocation + glm::rotate(merged.UserOffset.Rotation.GetQuat(), childPos - bodyLocation);

            const glm::mat4 lookAt = glm::inverse(glm::lookAt(bodyLocation, childPos, glm::vec3(0, 1, 0)));
            const PxQuat q = PhysXUtils::ToPhysXQuat(Math::DecomposeTransformMatrix(lookAt).Rotation.GetQuat());

            PxRigidDynamic* body = physics.createRigidDynamic(PxTransform(PhysXUtils::ToPhysXVector(bodyLocation), q));
            body->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
            body->userData = &payload;
            scene->addActor(*body);
            if (!parentBody)
            {
                // Create a parent to hold together all other bodies
                parentBody = PxCloneDynamic(physics, body->getGlobalPose(), *body);
                scene->addActor(*parentBody);
                physicsBoneData.Body = parentBody;
                physicsBoneData.Name = merged.Name;
                physicsBoneData.BoneWorldTr = compWorldTrInv * boneWorldTransform;
                physicsBoneData.OriginalBodyTrInv = glm::inverse(Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(parentBody->getGlobalPose())));
                parentBody->userData = &payload;
                parentBody->setSolverIterationCounts(settings.SolverIterations, settings.SolverVelocityIterations);
            }

            // Setup collider
            const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), PxHalfPi, glm::vec3(0.0f, 1.0f, 0.0f));
            PxShape* shape = physics.createShape(PxCapsuleGeometry(radius * merged.UserOffset.Scale3D.x, halfHeight * merged.UserOffset.Scale3D.y), material);
            shape->setSimulationFilterData(s_FilterData);
            PxTransform local(PhysXUtils::ToPhysXQuat(glm::quat_cast(rot)));
            shape->setLocalPose(local);
            body->attachShape(*shape);
            body->setLinearVelocity(linearVelocity);
            body->setAngularVelocity(angularVelocity);
            body->setActorFlag(PxActorFlag::eVISUALIZATION, true);

            // Setup Joint
            PxD6Joint* joint = PxD6JointCreate(physics,
                parentBody,
                parentBody->getGlobalPose().transformInv(jointTransform),
                body,
                body->getGlobalPose().transformInv(jointTransform));

            //joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);
            joint->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
            joint->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
            joint->setSwingLimit(physx::PxJointLimitCone(swing, swing));
            joint->setTwistLimit(physx::PxJointAngularLimitPair(-twist, twist));

            auto& childData = physicsBoneData.Children.emplace_back();
            childData.Shape = shape;
            childData.Body = body;
            childData.Joint = joint;
            childData.Name = merged.Name;
            childData.BoneWorldTr = compWorldTrInv * boneWorldTransform;
            childData.OriginalBodyTrInv = glm::inverse(compWorldTrInv * Math::ToTransformMatrix(PhysXUtils::FromPhysXTransform(body->getGlobalPose())));

            for (const auto& child : merged.Children)
                CreateArticulationChain(child, currentPose, boneMap, scene, childData, payload, settings, worldTransform, compWorldTrInv, material, twist, swing, linearVelocity, angularVelocity, body);
        }
    }

    static void UpdatePose(const PhysicsRagdollActor::BoneData& node, SkeletalPose& pose, const glm::mat4& origBaseWorldTrInv)
    {
        const auto& nodeToReactTo = node;// node.Children.empty() ? node : node.Children[0];
        Transform transform = PhysXUtils::FromPhysXTransform(nodeToReactTo.Body->getGlobalPose());
        glm::mat4 currentWorldTr = origBaseWorldTrInv * Math::ToTransformMatrix(transform);
        glm::mat4 offsetTr = currentWorldTr * nodeToReactTo.OriginalBodyTrInv;
        pose.Bones[node.Name] = Math::DecomposeTransformMatrix(offsetTr * node.BoneWorldTr);

        for (const auto& child : node.Children)
        {
            UpdatePose(child, pose, origBaseWorldTrInv);
        }
    }

    static void Release(PhysicsRagdollActor::BoneData& node)
    {
        if (node.Shape)
        {
            node.Shape->release();
            node.Shape = nullptr;
        }
        if (node.Body)
        {
            node.Body->release();
            node.Body = nullptr;
        }
        if (node.Joint)
        {
            node.Joint->release();
            node.Joint = nullptr;
        }

        for (auto& child : node.Children)
        {
            Release(child);
        }
        node = {};
    }

    static void SetShowCollision_Internal(const PhysicsRagdollActor::BoneData& node, bool bShow)
    {
        if (node.Body)
        {
            node.Body->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);
        }

        for (const auto& child : node.Children)
            SetShowCollision_Internal(child, bShow);
    }

    PhysicsRagdollActor::PhysicsRagdollActor(Entity entity, physx::PxScene* scene, const PhysicsSettings& settings)
		: m_Entity(entity), m_Scene(scene), m_Settings(settings)
	{
        if (!m_Entity.HasComponent<SkeletalMeshComponent>())
        {
            EG_CORE_ERROR("Failed to create a ragdoll actor. SkeletalMeshComponent is not present (Entity: {})", m_Entity.GetName());
            return;
        }

		auto& skeletalComp = m_Entity.GetComponent<SkeletalMeshComponent>();
		const auto& asset = skeletalComp.GetMeshAsset();
		if (!asset)
		{
            EG_CORE_ERROR("Failed to create a ragdoll actor. Skeletal mesh asset is not present (Entity: {})", m_Entity.GetName());
			return;
		}

		auto& physics = PhysXInternal::GetPhysics();
        const auto& mesh = asset->GetMesh();
		const auto& meshInfo = mesh->GetSkeletalMeshInfo();
        const float twist = mesh->GetRagdollMaxTwist();
        const float swing = mesh->GetRagdollMaxSwing();
		auto& rootNode = meshInfo.RootBone;
        m_Payload.Ptr = this;
        m_Payload.bRagdoll = true;

        const glm::mat4 worldTransform = Math::ToTransformMatrix(skeletalComp.GetWorldTransform());
        m_OriginalTransformInv = glm::inverse(worldTransform);
        if (skeletalComp.LastPose.Bones.empty())
        {
            // If LastPose is empty, fill it with base pose data
            const glm::mat4 rootTransform = glm::mat4(1.f);
            AnimationSystem::FinalizePose(skeletalComp.LastPose, rootNode, rootTransform);
        }

        physx::PxVec3 linearVelocity(0.f);
        physx::PxVec3 angularVelocity(0.f);
        if (const auto& actor = m_Entity.GetPhysicsActor())
        {
            if (actor->IsDynamic())
            {
                linearVelocity = PhysXUtils::ToPhysXVector(actor->GetLinearVelocity());
                angularVelocity = PhysXUtils::ToPhysXVector(actor->GetAngularVelocity());
            }
        }
        
        m_Material = physics.createMaterial(0.5f, 0.5f, 0.6f);
		CreateArticulationChain(mesh->GetRagdollRoot(), skeletalComp.LastPose, meshInfo.BoneInfoMap, m_Scene, m_Root, m_Payload, m_Settings, worldTransform, m_OriginalTransformInv, *m_Material,
            glm::radians(twist), glm::radians(swing), linearVelocity, angularVelocity);
	}

    PhysicsRagdollActor::~PhysicsRagdollActor()
    {
        const bool bExists = m_Root.Body != nullptr;
        if (bExists && m_Entity.HasComponent<SkeletalMeshComponent>())
        {
            m_Entity.GetComponent<SkeletalMeshComponent>().LastPose.Reset();
        }
        m_Material->release();
        m_Material = nullptr;
        Release(m_Root);
    }
    
    void PhysicsRagdollActor::SynchronizeTransform()
    {
        const bool bInvalid = m_Root.Body == nullptr;
        if (bInvalid)
            return;

        auto& skeletalComp = m_Entity.GetComponent<SkeletalMeshComponent>();
        auto& lastPose = skeletalComp.LastPose;
        lastPose.Reset();

        UpdatePose(m_Root, lastPose, m_OriginalTransformInv);
        m_bDirtyTransform = false;
    }
    
    void PhysicsRagdollActor::SetShowCollision(bool bShowCollision)
    {
        SetShowCollision_Internal(m_Root, bShowCollision);
    }
}
