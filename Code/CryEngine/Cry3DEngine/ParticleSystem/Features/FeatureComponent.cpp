// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParticleSystem/ParticleComponentRuntime.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentEnableIf

class CFeatureComponentEnableIf : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual bool CanMakeRuntime(CParticleEmitter* pEmitter) const override
	{
		return m_attribute.GetValueAs(pEmitter->GetAttributeInstance(), true);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_attribute, "Attribute", "Attribute");
	}

private:
	CAttributeReference m_attribute;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentEnableIf, "Component", "EnableIf", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentSpawnIf

class CFeatureComponentSpawnIf : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->MainPreUpdate.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_attribute, "Attribute", "Attribute");
	}

	virtual void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		if (!m_attribute.GetValueAs(pComponentRuntime->GetEmitter()->GetAttributeInstance(), true))
			pComponentRuntime->RemoveAllSubInstances();
	}

private:
	CAttributeReference m_attribute;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentSpawnIf, "Component", "SpawnIf", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentEnableByConfig

SERIALIZATION_DECLARE_ENUM(EEnableIfConfig,
	Low      = CONFIG_LOW_SPEC,
	Medium   = CONFIG_MEDIUM_SPEC,
	High     = CONFIG_HIGH_SPEC,
	VeryHigh = CONFIG_VERYHIGH_SPEC
	)

class CFeatureComponentEnableByConfig : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	CFeatureComponentEnableByConfig()
		: m_minimumConfig(EEnableIfConfig::Low)
		, m_maximumConfig(EEnableIfConfig::VeryHigh)
		, m_PC(true), m_XBoxOne(true), m_PS4(true) {}

	virtual bool CanMakeRuntime(CParticleEmitter* pEmitter) const override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const uint particleSpec = pEmitter->GetParticleSpec();
		const bool isPc = particleSpec <= CONFIG_VERYHIGH_SPEC;

		if (isPc)
		{
			return m_PC &&
				(particleSpec >= uint(m_minimumConfig)) &&
				(particleSpec <= uint(m_maximumConfig));
		}
		else if (particleSpec == CONFIG_DURANGO && m_XBoxOne)
			return true;
		else if (particleSpec == CONFIG_ORBIS && m_PS4)
			return true;

		return false;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_PC, "PC", "PC");
		if (m_PC)
		{
			ar(m_minimumConfig, "Minimum", "Minimum");
			ar(m_maximumConfig, "Maximum", "Maximum");
		}
		ar(m_XBoxOne, "XBoxOne", "XBox One");
		ar(m_PS4, "PS4", "Playstation 4");
	}

private:
	EEnableIfConfig m_minimumConfig;
	EEnableIfConfig m_maximumConfig;
	bool m_PC, m_XBoxOne, m_PS4;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentEnableByConfig, "Component", "EnableByConfig", colorComponent);

}
