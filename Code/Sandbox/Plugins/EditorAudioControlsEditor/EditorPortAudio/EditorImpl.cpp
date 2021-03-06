// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include "Connection.h"
#include "ProjectLoader.h"

#include <CryAudioImplPortAudio/GlobalData.h>
#include <CrySystem/ISystem.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/ClassFactory.h>

namespace ACE
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
CSettings::CSettings()
	: m_assetAndProjectPath(AUDIO_SYSTEM_DATA_ROOT "/" + string(CryAudio::Impl::PortAudio::s_szImplFolderName) + "/" + string(CryAudio::s_szAssetsFolderName))
{}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::CEditorImpl()
{
	gEnv->pAudioSystem->GetImplInfo(m_implInfo);
	m_implName = m_implInfo.name.c_str();
	m_implFolderName = CryAudio::Impl::PortAudio::s_szImplFolderName;
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::~CEditorImpl()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Reload(bool const preserveConnectionStatus)
{
	Clear();

	CProjectLoader(GetSettings()->GetProjectPath(), m_rootItem);

	CreateItemCache(&m_rootItem);

	if (preserveConnectionStatus)
	{
		for (auto const& connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				auto const pItem = static_cast<CItem* const>(GetItem(connection.first));

				if (pItem != nullptr)
				{
					pItem->SetConnected(true);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
IImplItem* CEditorImpl::GetItem(ControlId const id) const
{
	IImplItem* pIImplItem = nullptr;

	if (id >= 0)
	{
		pIImplItem = stl::find_in_map(m_itemCache, id, nullptr);
	}

	return pIImplItem;
}

//////////////////////////////////////////////////////////////////////////
char const* CEditorImpl::GetTypeIcon(IImplItem const* const pIImplItem) const
{
	char const* szIconPath = "icons:Dialogs/dialog-error.ico";
	auto const pItem = static_cast<CItem const* const>(pIImplItem);

	if (pItem != nullptr)
	{
		EItemType const type = pItem->GetType();

		switch (type)
		{
		case EItemType::Event:
			szIconPath = "icons:audio/portaudio/event.ico";
			break;
		case EItemType::Folder:
			szIconPath = "icons:General/Folder.ico";
			break;
		default:
			szIconPath = "icons:Dialogs/dialog-error.ico";
			break;
		}
	}

	return szIconPath;
}

//////////////////////////////////////////////////////////////////////////
string const& CEditorImpl::GetName() const
{
	return m_implName;
}

//////////////////////////////////////////////////////////////////////////
string const& CEditorImpl::GetFolderName() const
{
	return m_implFolderName;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsSystemTypeSupported(EAssetType const assetType) const
{
	bool isSupported = false;

	switch (assetType)
	{
	case EAssetType::Trigger:
	case EAssetType::Folder:
	case EAssetType::Library:
		isSupported = true;
		break;
	default:
		isSupported = false;
		break;
	}

	return isSupported;
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsTypeCompatible(EAssetType const assetType, IImplItem const* const pIImplItem) const
{
	bool isCompatible = false;
	auto const pItem = static_cast<CItem const* const>(pIImplItem);

	if (pItem != nullptr)
	{
		if (assetType == EAssetType::Trigger)
		{
			isCompatible = (pItem->GetType() == EItemType::Event);
		}
	}

	return isCompatible;
}

//////////////////////////////////////////////////////////////////////////
EAssetType CEditorImpl::ImplTypeToAssetType(IImplItem const* const pIImplItem) const
{
	EAssetType assetType = EAssetType::None;
	auto const pItem = static_cast<CItem const* const>(pIImplItem);

	if (pItem != nullptr)
	{
		EItemType const implType = pItem->GetType();

		switch (implType)
		{
		case EItemType::Event:
			assetType = EAssetType::Trigger;
			break;
		default:
			assetType = EAssetType::None;
			break;
		}
	}

	return assetType;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionToControl(EAssetType const assetType, IImplItem const* const pIImplItem)
{
	ConnectionPtr pConnection = nullptr;

	if (pIImplItem != nullptr)
	{
		pConnection = std::make_shared<CEventConnection>(pIImplItem->GetId());
	}

	return pConnection;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionFromXMLNode(XmlNodeRef pNode, EAssetType const assetType)
{
	ConnectionPtr pConnectionPtr = nullptr;

	if (pNode != nullptr)
	{
		char const* const szTag = pNode->getTag();

		if ((_stricmp(szTag, CryAudio::s_szEventTag) == 0) ||
		    (_stricmp(szTag, CryAudio::Impl::PortAudio::s_szFileTag) == 0) ||
		    (_stricmp(szTag, "PortAudioEvent") == 0) || // Backwards compatibility.
		    (_stricmp(szTag, "PortAudioSample") == 0))  // Backwards compatibility.
		{
			string name = pNode->getAttr(CryAudio::s_szNameAttribute);
			string path = pNode->getAttr(CryAudio::Impl::PortAudio::s_szPathAttribute);
			// Backwards compatibility will be removed before March 2019.
#if defined (USE_BACKWARDS_COMPATIBILITY)
			if (name.IsEmpty() && pNode->haveAttr("portaudio_name"))
			{
				name = pNode->getAttr("portaudio_name");
			}

			if (path.IsEmpty() && pNode->haveAttr("portaudio_path"))
			{
				path = pNode->getAttr("portaudio_path");
			}
#endif    // USE_BACKWARDS_COMPATIBILITY
			ControlId id;

			if (path.empty())
			{
				id = GetId(name);
			}
			else
			{
				id = GetId(path + "/" + name);
			}

			auto pItem = static_cast<CItem*>(GetItem(id));

			if (pItem == nullptr)
			{
				pItem = new CItem(name, id, EItemType::Event, EItemFlags::IsPlaceHolder);
				m_itemCache[id] = pItem;
			}

			if (pItem != nullptr)
			{
				auto const pConnection = std::make_shared<CEventConnection>(pItem->GetId());
				string actionType = pNode->getAttr(CryAudio::s_szTypeAttribute);
#if defined (USE_BACKWARDS_COMPATIBILITY)
				if (actionType.IsEmpty() && pNode->haveAttr("event_type"))
				{
					actionType = pNode->getAttr("event_type");
				}
#endif      // USE_BACKWARDS_COMPATIBILITY
				pConnection->m_actionType = actionType.compareNoCase(CryAudio::Impl::PortAudio::s_szStopValue) == 0 ? EEventActionType::Stop : EEventActionType::Start;

				int loopCount = 0;
				pNode->getAttr(CryAudio::Impl::PortAudio::s_szLoopCountAttribute, loopCount);
				loopCount = std::max(0, loopCount);
				pConnection->m_loopCount = static_cast<uint32>(loopCount);

				if (pConnection->m_loopCount == 0)
				{
					pConnection->m_isInfiniteLoop = true;
				}

				pConnectionPtr = pConnection;
			}
		}
	}

	return pConnectionPtr;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CEditorImpl::CreateXMLNodeFromConnection(ConnectionPtr const pConnection, EAssetType const assetType)
{
	XmlNodeRef pConnectionNode = nullptr;

	std::shared_ptr<CEventConnection const> const pImplConnection = std::static_pointer_cast<CEventConnection const>(pConnection);
	auto const pItem = static_cast<CItem const* const>(GetItem(pConnection->GetID()));

	if ((pItem != nullptr) && (pImplConnection != nullptr) && (assetType == EAssetType::Trigger))
	{
		pConnectionNode = GetISystem()->CreateXmlNode(CryAudio::s_szEventTag);
		pConnectionNode->setAttr(CryAudio::s_szNameAttribute, pItem->GetName());

		string path;
		IImplItem const* pParent = pItem->GetParent();

		while (pParent != nullptr)
		{
			string const parentName = pParent->GetName();

			if (!parentName.empty())
			{
				if (path.empty())
				{
					path = parentName;
				}
				else
				{
					path = parentName + "/" + path;
				}
			}

			pParent = pParent->GetParent();
		}

		pConnectionNode->setAttr(CryAudio::Impl::PortAudio::s_szPathAttribute, path);

		if (pImplConnection->m_actionType == EEventActionType::Start)
		{
			pConnectionNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::PortAudio::s_szStartValue);

			if (pImplConnection->m_isInfiniteLoop)
			{
				pConnectionNode->setAttr(CryAudio::Impl::PortAudio::s_szLoopCountAttribute, 0);
			}
			else
			{
				pConnectionNode->setAttr(CryAudio::Impl::PortAudio::s_szLoopCountAttribute, pImplConnection->m_loopCount);
			}
		}
		else
		{
			pConnectionNode->setAttr(CryAudio::s_szTypeAttribute, CryAudio::Impl::PortAudio::s_szStopValue);
		}
	}

	return pConnectionNode;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::EnableConnection(ConnectionPtr const pConnection)
{
	auto const pItem = static_cast<CItem* const>(GetItem(pConnection->GetID()));

	if (pItem != nullptr)
	{
		++m_connectionsByID[pItem->GetId()];
		pItem->SetConnected(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DisableConnection(ConnectionPtr const pConnection)
{
	auto const pItem = static_cast<CItem* const>(GetItem(pConnection->GetID()));

	if (pItem != nullptr)
	{
		int connectionCount = m_connectionsByID[pItem->GetId()] - 1;

		if (connectionCount < 1)
		{
			CRY_ASSERT_MESSAGE(connectionCount >= 0, "Connection count is < 0");
			connectionCount = 0;
			pItem->SetConnected(false);
		}

		m_connectionsByID[pItem->GetId()] = connectionCount;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Clear()
{
	for (auto const& itemPair : m_itemCache)
	{
		delete itemPair.second;
	}

	m_itemCache.clear();
	m_rootItem.Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CreateItemCache(CItem const* const pParent)
{
	if (pParent != nullptr)
	{
		size_t const count = pParent->GetNumChildren();

		for (size_t i = 0; i < count; ++i)
		{
			auto const pChild = static_cast<CItem* const>(pParent->GetChildAt(i));

			if (pChild != nullptr)
			{
				m_itemCache[pChild->GetId()] = pChild;
				CreateItemCache(pChild);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ControlId CEditorImpl::GetId(string const& name) const
{
	return CryAudio::StringToId(name);
}
} // namespace PortAudio
} // namespace ACE

