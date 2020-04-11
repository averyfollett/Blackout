// Copyright (c), Firelight Technologies Pty, Ltd. 2012-2020.

#include "FMODAssetTable.h"
#include "FMODEvent.h"
#include "FMODSnapshot.h"
#include "FMODSnapshotReverb.h"
#include "FMODBank.h"
#include "FMODBus.h"
#include "FMODVCA.h"
#include "FMODUtils.h"
#include "FMODSettings.h"
#include "FMODFileCallbacks.h"
#include "FMODStudioPrivatePCH.h"
#include "fmod_studio.hpp"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "AssetRegistryModule.h"
#endif

FFMODAssetTable::FFMODAssetTable()
    : StudioSystem(nullptr)
{
}

FFMODAssetTable::~FFMODAssetTable()
{
    Destroy();
}

void FFMODAssetTable::Create()
{
    Destroy();

    // Create a sandbox system purely for loading and considering banks
    verifyfmod(FMOD::Studio::System::create(&StudioSystem));
    FMOD::System *lowLevelSystem = nullptr;
    verifyfmod(StudioSystem->getCoreSystem(&lowLevelSystem));
    verifyfmod(lowLevelSystem->setOutput(FMOD_OUTPUTTYPE_NOSOUND));
    AttachFMODFileSystem(lowLevelSystem, 2048);
    verifyfmod(
        StudioSystem->initialize(1, FMOD_STUDIO_INIT_ALLOW_MISSING_PLUGINS | FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE, FMOD_INIT_MIX_FROM_UPDATE, 0));
}

void FFMODAssetTable::Destroy()
{
    if (StudioSystem != nullptr)
    {
        verifyfmod(StudioSystem->release());
    }
    StudioSystem = nullptr;
}

UFMODAsset *FFMODAssetTable::FindByName(const FString &Name) const
{
    const TWeakObjectPtr<UFMODAsset> *FoundAsset = FullNameLookup.Find(Name);
    if (FoundAsset)
    {
        return FoundAsset->Get();
    }
    return nullptr;
}

void FFMODAssetTable::Refresh()
{
    if (StudioSystem == nullptr)
    {
        return;
    }

    BuildBankPathLookup();

    if (!MasterStringsBankPath.IsEmpty())
    {
        const UFMODSettings &Settings = *GetDefault<UFMODSettings>();
        FString StringPath = Settings.GetFullBankPath() / MasterStringsBankPath;

        UE_LOG(LogFMOD, Log, TEXT("Loading strings bank: %s"), *StringPath);

        FMOD::Studio::Bank *StudioStringBank;
        FMOD_RESULT StringResult = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*StringPath), FMOD_STUDIO_LOAD_BANK_NORMAL, &StudioStringBank);
        if (StringResult == FMOD_OK)
        {
            TArray<char> RawBuffer;
            RawBuffer.SetNum(256); // Initial capacity

            int Count = 0;
            verifyfmod(StudioStringBank->getStringCount(&Count));
            for (int StringIdx = 0; StringIdx < Count; ++StringIdx)
            {
                FMOD_RESULT Result;
                FMOD::Studio::ID Guid = { 0 };
                while (true)
                {
                    int ActualSize = 0;
                    Result = StudioStringBank->getStringInfo(StringIdx, &Guid, RawBuffer.GetData(), RawBuffer.Num(), &ActualSize);
                    if (Result == FMOD_ERR_TRUNCATED)
                    {
                        RawBuffer.SetNum(ActualSize);
                    }
                    else
                    {
                        break;
                    }
                }
                verifyfmod(Result);
                FString AssetName(UTF8_TO_TCHAR(RawBuffer.GetData()));
                FGuid AssetGuid = FMODUtils::ConvertGuid(Guid);
                if (!AssetName.IsEmpty())
                {
                    AddAsset(AssetGuid, AssetName);
                }
            }
            verifyfmod(StudioStringBank->unload());
            verifyfmod(StudioSystem->update());
        }
        else
        {
            UE_LOG(LogFMOD, Warning, TEXT("Failed to load strings bank: %s"), *StringPath);
        }
    }
}

void FFMODAssetTable::AddAsset(const FGuid &AssetGuid, const FString &AssetFullName)
{
    FString AssetPath = AssetFullName;
    FString AssetType = "";
    FString AssetFileName = "asset";

    int DelimIndex;
    if (AssetPath.FindChar(':', DelimIndex))
    {
        AssetType = AssetPath.Left(DelimIndex);
        AssetPath = AssetPath.Right(AssetPath.Len() - DelimIndex - 1);
    }

    FString FormattedAssetType = "";
    UClass *AssetClass = UFMODAsset::StaticClass();
    if (AssetType.Equals(TEXT("event")))
    {
        FormattedAssetType = TEXT("Events");
        AssetClass = UFMODEvent::StaticClass();
    }
    else if (AssetType.Equals(TEXT("snapshot")))
    {
        FormattedAssetType = TEXT("Snapshots");
        AssetClass = UFMODSnapshot::StaticClass();
    }
    else if (AssetType.Equals(TEXT("bank")))
    {
        FormattedAssetType = TEXT("Banks");
        AssetClass = UFMODBank::StaticClass();
    }
    else if (AssetType.Equals(TEXT("bus")))
    {
        FormattedAssetType = TEXT("Buses");
        AssetClass = UFMODBus::StaticClass();
    }
    else if (AssetType.Equals(TEXT("vca")))
    {
        FormattedAssetType = TEXT("VCAs");
        AssetClass = UFMODVCA::StaticClass();
    }
    else if (AssetType.Equals(TEXT("parameter")))
    {
        return;
    }
    else
    {
        UE_LOG(LogFMOD, Warning, TEXT("Unknown asset type: %s"), *AssetType);
    }

    if (AssetPath.FindLastChar('/', DelimIndex))
    {
        AssetFileName = AssetPath.Right(AssetPath.Len() - DelimIndex - 1);
        AssetPath = AssetPath.Left(AssetPath.Len() - AssetFileName.Len() - 1);
    }
    else
    {
        // No path part, all name
        AssetFileName = AssetPath;
        AssetPath = TEXT("");
    }

    if (AssetFileName.IsEmpty() || AssetFileName.Contains(TEXT(".strings")))
    {
        UE_LOG(LogFMOD, Log, TEXT("Skipping asset: %s"), *AssetFullName);
        return;
    }

    AssetPath = AssetPath.Replace(TEXT(" "), TEXT("_"));
    FString AssetShortName = AssetFileName.Replace(TEXT(" "), TEXT("_"));
    AssetShortName = AssetShortName.Replace(TEXT("."), TEXT("_"));

    const UFMODSettings &Settings = *GetDefault<UFMODSettings>();

    FString FolderPath = Settings.ContentBrowserPrefix;
    FolderPath += FormattedAssetType;
    FolderPath += AssetPath;

    FString AssetPackagePath = FolderPath + TEXT("/") + AssetShortName;

    FName AssetPackagePathName(*AssetPackagePath);

    TWeakObjectPtr<UFMODAsset> &ExistingNameAsset = NameMap.FindOrAdd(AssetPackagePathName);
    TWeakObjectPtr<UFMODAsset> &ExistingGuidAsset = GuidMap.FindOrAdd(AssetGuid);
    TWeakObjectPtr<UFMODAsset> &ExistingFullNameLookupAsset = FullNameLookup.FindOrAdd(AssetFullName);

    UFMODAsset *AssetNameObject = ExistingNameAsset.Get();
    if (AssetNameObject == nullptr)
    {
        UE_LOG(LogFMOD, Log, TEXT("Constructing asset: %s"), *AssetPackagePath);

        EObjectFlags NewObjectFlags = RF_Standalone | RF_Public /* | RF_Transient */;
        if (IsRunningDedicatedServer())
        {
            NewObjectFlags |= RF_MarkAsRootSet;
        }

        UPackage *NewPackage = CreatePackage(nullptr, *AssetPackagePath);
        if (IsValid(NewPackage))
        {
            if (!GEventDrivenLoaderEnabled)
            {
                NewPackage->SetPackageFlags(PKG_CompiledIn);
            }

            AssetNameObject = NewObject<UFMODAsset>(NewPackage, AssetClass, FName(*AssetShortName), NewObjectFlags);
            AssetNameObject->AssetGuid = AssetGuid;
            AssetNameObject->bShowAsAsset = true;
            AssetNameObject->FileName = AssetFileName;

#if WITH_EDITOR
            FAssetRegistryModule &AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            AssetRegistryModule.Get().AddPath(*FolderPath);
            FAssetRegistryModule::AssetCreated(AssetNameObject);
#endif
        }
        else
        {
            UE_LOG(LogFMOD, Warning, TEXT("Failed to construct package for asset %s"), *AssetPackagePath);
        }

        if (AssetClass == UFMODSnapshot::StaticClass())
        {
            FString ReverbFolderPath = Settings.ContentBrowserPrefix;
            ReverbFolderPath += TEXT("Reverbs");
            ReverbFolderPath += AssetPath;

            FString ReverbAssetPackagePath = ReverbFolderPath + TEXT("/") + AssetShortName;

            UPackage *ReverbPackage = CreatePackage(nullptr, *ReverbAssetPackagePath);
            if (ReverbPackage)
            {
                if (!GEventDrivenLoaderEnabled)
                {
                    ReverbPackage->SetPackageFlags(PKG_CompiledIn);
                }
                UFMODSnapshotReverb *AssetReverb = NewObject<UFMODSnapshotReverb>(
                    ReverbPackage, UFMODSnapshotReverb::StaticClass(), FName(*AssetShortName), NewObjectFlags);
                AssetReverb->AssetGuid = AssetGuid;
                AssetReverb->bShowAsAsset = true;

#if WITH_EDITOR
                FAssetRegistryModule &AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
                AssetRegistryModule.Get().AddPath(*ReverbFolderPath);
                FAssetRegistryModule::AssetCreated(AssetReverb);
#endif
            }
        }
    }

    UFMODAsset *AssetGuidObject = ExistingGuidAsset.Get();
    if (IsValid(AssetGuidObject) && AssetGuidObject != AssetNameObject)
    {
        FString OldPath = AssetGuidObject->GetPathName();
        UE_LOG(LogFMOD, Log, TEXT("Hiding old asset '%s'"), *OldPath);

        // We had an asset with the same guid but it must have been renamed
        // We just hide the old asset from the asset table
        AssetGuidObject->bShowAsAsset = false;

#if WITH_EDITOR
        FAssetRegistryModule::AssetRenamed(AssetNameObject, OldPath);
#endif
    }

    ExistingNameAsset = AssetNameObject;
    ExistingGuidAsset = AssetNameObject;
    ExistingFullNameLookupAsset = AssetNameObject;
}

FString FFMODAssetTable::GetBankPathByGuid(const FGuid& Guid) const
{
    FString BankPath = "";
    const FString* File = nullptr;
    const BankLocalizations* localizations = BankPathLookup.Find(Guid);

    if (localizations)
    {
        const FString* DefaultFile = nullptr;

        for (int i = 0; i < localizations->Num(); ++i)
        {
            if ((*localizations)[i].Locale.IsEmpty())
            {
                DefaultFile = &(*localizations)[i].Path;
            }
            else if ((*localizations)[i].Locale == ActiveLocale)
            {
                File = &(*localizations)[i].Path;
                break;
            }
        }

        if (!File)
        {
            File = DefaultFile;
        }
    }

    if (File)
    {
        BankPath = *File;
    }

    return BankPath;
}

FString FFMODAssetTable::GetBankPath(const UFMODBank &Bank) const
{
    FString BankPath = GetBankPathByGuid(Bank.AssetGuid);

    if (BankPath.IsEmpty())
    {
        UE_LOG(LogFMOD, Warning, TEXT("Could not find disk file for bank %s"), *Bank.FileName);
    }

    return BankPath;
}

FString FFMODAssetTable::GetMasterBankPath() const
{
    return MasterBankPath;
}

FString FFMODAssetTable::GetMasterStringsBankPath() const
{
    return MasterStringsBankPath;
}

FString FFMODAssetTable::GetMasterAssetsBankPath() const
{
    return MasterAssetsBankPath;
}

void FFMODAssetTable::SetLocale(const FString &LocaleCode)
{
    ActiveLocale = LocaleCode;
}

void FFMODAssetTable::GetAllBankPaths(TArray<FString> &Paths, bool IncludeMasterBank) const
{
    const UFMODSettings &Settings = *GetDefault<UFMODSettings>();

    for (const TMap<FGuid, BankLocalizations>::ElementType& Localizations : BankPathLookup)
    {
        FString BankPath = GetBankPathByGuid(Localizations.Key);
        bool Skip = false;

        if (BankPath.IsEmpty())
        {
            // Never expect to be in here, but should skip empty paths
            continue;
        }

        if (!IncludeMasterBank)
        {
            Skip = (BankPath == Settings.GetMasterBankFilename() || BankPath == Settings.GetMasterAssetsBankFilename() || BankPath == Settings.GetMasterStringsBankFilename());
        }

        if (!Skip)
        {
            Paths.Push(Settings.GetFullBankPath() / BankPath);
        }
    }
}


void FFMODAssetTable::GetAllBankPathsFromDisk(const FString &BankDir, TArray<FString> &Paths)
{
    FString SearchDir = BankDir;

    TArray<FString> AllFiles;
    IFileManager::Get().FindFilesRecursive(AllFiles, *SearchDir, TEXT("*.bank"), true, false, false);

    for (FString &CurFile : AllFiles)
    {
        Paths.Push(CurFile);
    }
}

void FFMODAssetTable::BuildBankPathLookup()
{
    const UFMODSettings &Settings = *GetDefault<UFMODSettings>();

    TArray<FString> BankPaths;
    GetAllBankPathsFromDisk(Settings.GetFullBankPath(), BankPaths);

    BankPathLookup.Empty(BankPaths.Num());
    MasterBankPath.Empty();
    MasterStringsBankPath.Empty();
    MasterAssetsBankPath.Empty();

    if (BankPaths.Num() == 0)
    {
        return;
    }

    for (FString BankPath : BankPaths)
    {
        FMOD::Studio::Bank *Bank;
        FMOD_RESULT result = StudioSystem->loadBankFile(TCHAR_TO_UTF8(*BankPath), FMOD_STUDIO_LOAD_BANK_NORMAL, &Bank);
        FMOD_GUID GUID;

        if (result == FMOD_OK)
        {
            result = Bank->getID(&GUID);
            Bank->unload();
        }

        if (result == FMOD_OK)
        {
            FString CurFilename = FPaths::GetCleanFilename(BankPath);
            FString PathPart;
            FString FilenamePart;
            FString ExtensionPart;
            FPaths::Split(BankPath, PathPart, FilenamePart, ExtensionPart);
            BankPath = BankPath.RightChop(Settings.GetFullBankPath().Len() + 1);

            BankLocalization localization;
            localization.Path = BankPath;
            localization.Locale = "";

            for (const FFMODProjectLocale& Locale : Settings.Locales)
            {
                if (FilenamePart.EndsWith(FString("_") + Locale.LocaleCode))
                {
                    localization.Locale = Locale.LocaleCode;
                    break;
                }
            }

            BankLocalizations& localizations = BankPathLookup.FindOrAdd(FMODUtils::ConvertGuid(GUID));
            localizations.Add(localization);

            if (MasterBankPath.IsEmpty() && CurFilename == Settings.GetMasterBankFilename())
            {
                MasterBankPath = BankPath;
            }
            else if (MasterStringsBankPath.IsEmpty() && CurFilename == Settings.GetMasterStringsBankFilename())
            {
                MasterStringsBankPath = BankPath;
            }
            else if (MasterAssetsBankPath.IsEmpty() && CurFilename == Settings.GetMasterAssetsBankFilename())
            {
                MasterAssetsBankPath = BankPath;
            }
        }

        if (result != FMOD_OK)
        {
            UE_LOG(LogFMOD, Error, TEXT("Failed to register disk file for bank: %s"), *BankPath);
        }
    }

    StudioSystem->flushCommands();
}
