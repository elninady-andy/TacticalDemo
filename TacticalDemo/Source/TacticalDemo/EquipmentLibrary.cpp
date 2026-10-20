#include "EquipmentLibrary.h"
#include "ScenarioConfigLoader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace
{
    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
    {
        const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
        return Value && (*Value)->Type == EJson::String && (*Value)->TryGetString(Out) && !Out.TrimStartAndEnd().IsEmpty();
    }
}

bool FEquipmentLibrary::Load(const FString& Path, FString& OutError)
{
    OutError.Reset();
    Models.Reset(); Defaults.Reset(); SideDefaults.Reset(); SideColors.Reset(); Meshes.Reset(); LoadedTextures.Reset(); TextureMaterials.Reset();
    FString Text;
    TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* ModelObject = nullptr;
    const TSharedPtr<FJsonObject>* DefaultObject = nullptr;
    if (!FFileHelper::LoadFileToString(Text, *Path)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()
        || !Root->TryGetObjectField(TEXT("models"), ModelObject) || !Root->TryGetObjectField(TEXT("defaults"), DefaultObject))
    {
        OutError = Path + TEXT(": expected readable JSON with models/defaults objects");
        return false;
    }
    for (const auto& Pair : (*ModelObject)->Values)
    {
        FEquipmentDefinition Definition;
        FString Kind;
        if (Pair.Key.TrimStartAndEnd().IsEmpty() || Pair.Value->Type != EJson::Object)
        {
            OutError = Path + TEXT(": models contains empty id or non-object entry");
            return false;
        }
        const auto Entry = Pair.Value->AsObject();
        if (!ReadString(Entry, TEXT("type"), Definition.Type) || !FScenarioConfigLoader::IsSupportedType(Definition.Type)
            || !ReadString(Entry, TEXT("kind"), Kind) || Kind != TEXT("staticMesh")
            || !ReadString(Entry, TEXT("asset"), Definition.Asset))
        {
            OutError = Path + TEXT(": models.") + Pair.Key + TEXT(" requires valid type, kind=staticMesh, nonempty asset");
            return false;
        }
        if(Entry->HasField(TEXT("scale")) && (!Entry->TryGetNumberField(TEXT("scale"),Definition.Scale) || !FMath::IsFinite(Definition.Scale) || Definition.Scale<=0 || Definition.Scale>1000))
        { OutError=Path+TEXT(": invalid model scale: ")+Pair.Key; return false; }
        if(Entry->HasField(TEXT("preserveMaterials")) && !Entry->TryGetBoolField(TEXT("preserveMaterials"),Definition.PreserveMaterials))
        { OutError=Path+TEXT(": preserveMaterials must be boolean: ")+Pair.Key; return false; }
        if(Entry->HasField(TEXT("textures")))
        {
            const TSharedPtr<FJsonObject>* Textures=nullptr; double Slot=0;
            if(!Entry->TryGetObjectField(TEXT("textures"),Textures) || (*Textures)->Values.IsEmpty() ||
                !ReadString(Entry,TEXT("textureMaterial"),Definition.TextureMaterial) ||
                !Entry->TryGetNumberField(TEXT("textureSlot"),Slot) || !FMath::IsFinite(Slot) || Slot<0 || Slot>63 || FMath::FloorToDouble(Slot)!=Slot)
            { OutError=Path+TEXT(": invalid texture registration: ")+Pair.Key; return false; }
            Definition.TextureSlot=static_cast<int32>(Slot);
            for(const auto& Texture:(*Textures)->Values)
            {
                if(Texture.Key.IsEmpty() || Texture.Value->Type!=EJson::String || Texture.Value->AsString().TrimStartAndEnd().IsEmpty())
                { OutError=Path+TEXT(": texture ids and assets must be nonempty strings"); return false; }
                Definition.Textures.Add(Texture.Key,Texture.Value->AsString());
            }
        }
        Models.Add(Pair.Key, MoveTemp(Definition));
    }
    for (const TCHAR* Type : {TEXT("aircraft"), TEXT("vehicle"), TEXT("radar"), TEXT("target")})
    {
        FString Id;
        if (!ReadString(*DefaultObject, Type, Id) || !Models.Contains(Id) || Models[Id].Type != Type)
        {
            OutError = Path + TEXT(": defaults.") + Type + TEXT(" must reference a registered model of the same type");
            return false;
        }
        Defaults.Add(Type, Id);
        if (!LoadMesh(Id))
        {
            OutError = Path + TEXT(": defaults.") + Type + TEXT(" cannot load static mesh: ") + Models[Id].Asset;
            return false;
        }
    }
    if(const TSharedPtr<FJsonValue>* SideValue=Root->Values.Find(TEXT("sideDefaults")); SideValue && (*SideValue)->Type==EJson::Object)
    {
        const TSharedPtr<FJsonObject> SideObject=(*SideValue)->AsObject();
        for(const auto& Pair:SideObject->Values)
        {
            if(Pair.Value->Type!=EJson::String) { OutError=Path+TEXT(": sideDefaults entries must be strings"); return false; }
            const FString Key=Pair.Key;
            const int32 Sep=Key.Find(TEXT("."));
            if(Sep<=0) { OutError=Path+TEXT(": sideDefaults key must be type.side"); return false; }
            const FString Type=Key.Left(Sep), Side=Key.Mid(Sep+1), Id=Pair.Value->AsString();
            if(!FScenarioConfigLoader::IsSupportedType(Type) || (Side!=TEXT("red") && Side!=TEXT("blue")) || !Models.Contains(Id) || Models[Id].Type!=Type)
            { OutError=Path+TEXT(": invalid sideDefaults.")+Key; return false; }
            SideDefaults.Add(Key,Id);
            if(!LoadMesh(Id)) { OutError=Path+TEXT(": side default cannot load mesh: ")+Id; return false; }
        }
    }
    if(const TSharedPtr<FJsonValue>* ColorValue=Root->Values.Find(TEXT("sideColors")); ColorValue && (*ColorValue)->Type==EJson::Object)
    {
        for(const auto& Pair:(*ColorValue)->AsObject()->Values)
        {
            FString Hex;
            if(Pair.Value->Type!=EJson::String || !Pair.Value->TryGetString(Hex) || (Hex.Len()!=6 && Hex.Len()!=8))
            { OutError=Path+TEXT(": sideColors values must be valid hex colors"); return false; }
            const FColor Color=FColor::FromHex(Hex);
            SideColors.Add(Pair.Key,FLinearColor(Color));
        }
    }
    return true;
}

bool FEquipmentLibrary::IsModelRegistered(const FString& Path, const FString& ModelId, const FString& Type, FString& OutError)
{
    OutError.Reset();
    FString Text;
    TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* ModelObject = nullptr;
    if (!FFileHelper::LoadFileToString(Text, *Path)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()
        || !Root->TryGetObjectField(TEXT("models"), ModelObject))
    {
        OutError = TEXT("无法读取设备库 models：") + Path;
        return false;
    }
    const TSharedPtr<FJsonValue>* Value = (*ModelObject)->Values.Find(ModelId);
    FString RegisteredType, Kind, Asset;
    if (!Value || (*Value)->Type != EJson::Object ||
        !ReadString((*Value)->AsObject(), TEXT("type"), RegisteredType) || RegisteredType != Type ||
        !ReadString((*Value)->AsObject(), TEXT("kind"), Kind) || Kind != TEXT("staticMesh") ||
        !ReadString((*Value)->AsObject(), TEXT("asset"), Asset))
    {
        OutError = FString::Printf(TEXT("模型 %s 未在设备库登记为 %s 类型的静态网格。"), *ModelId, *Type);
        return false;
    }
    return true;
}

FString FEquipmentLibrary::ModelCatalog(const FString& Path)
{
    FString Text;
    TSharedPtr<FJsonObject> Root;
    const TSharedPtr<FJsonObject>* ModelObject = nullptr;
    if (!FFileHelper::LoadFileToString(Text, *Path)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()
        || !Root->TryGetObjectField(TEXT("models"), ModelObject)) return FString();
    TArray<FString> Entries;
    for (const auto& Pair : (*ModelObject)->Values)
    {
        FString Type;
        if (Pair.Value->Type == EJson::Object && ReadString(Pair.Value->AsObject(), TEXT("type"), Type))
            Entries.Add(FString::Printf(TEXT("%s(%s)"), *Pair.Key, *Type));
    }
    Entries.Sort();
    return FString::Join(Entries, TEXT("、"));
}

bool FEquipmentLibrary::ValidateModelChanges(const FString& CurrentScenario, const FString& CandidateScenario, const FString& LibraryPath, FString& OutError)
{
    FDemoScenarioConfig Current, Candidate;
    if (!FScenarioConfigLoader::Parse(CurrentScenario, Current, OutError)) return false;
    if (!FScenarioConfigLoader::Parse(CandidateScenario, Candidate, OutError)) return false;
    TMap<FString, const FDemoUnitConfig*> Existing;
    for (const FDemoUnitConfig& Group : Current.Units) Existing.Add(Group.Id, &Group);
    for (const FDemoUnitConfig& Group : Candidate.Units)
    {
        const FDemoUnitConfig* Previous = Existing.FindRef(Group.Id);
        if (Previous && Previous->ModelId == Group.ModelId && Previous->Type == Group.Type) continue;
        if (!IsModelRegistered(LibraryPath, Group.ModelId, Group.Type, OutError))
        {
            OutError = TEXT("候选配置中的单位组 ") + Group.Id + TEXT("：") + OutError;
            return false;
        }
    }
    FEquipmentLibrary Library;
    if(!Library.Load(LibraryPath,OutError)) return false;
    for(const FDemoUnitConfig& Group:Candidate.Units)
    {
        UTexture* Texture=nullptr; UMaterialInterface* Material=nullptr; int32 Slot=0;
        if(!Library.ResolveTexture(Group.ModelId,Group.Type,Group.Side,Group.TextureId,Texture,Material,Slot,OutError)) return false;
    }
    return true;
}

FLinearColor FEquipmentLibrary::ColorForSide(const FString& Side) const
{
    if(const FLinearColor* Found=SideColors.Find(Side)) return *Found;
    return Side==TEXT("red") ? FLinearColor(1.f,0.04f,0.025f,1.f) : FLinearColor(0.04f,0.25f,1.f,1.f);
}

FString FEquipmentLibrary::TextureCatalog(const FString& Path)
{
    FString Text; TSharedPtr<FJsonObject> Root; const TSharedPtr<FJsonObject>* ModelsObject=nullptr;
    if(!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid() || !Root->TryGetObjectField(TEXT("models"),ModelsObject)) return FString();
    TArray<FString> Entries;
    for(const auto& Model:(*ModelsObject)->Values)
    {
        const TSharedPtr<FJsonObject>* Textures=nullptr;
        if(Model.Value->Type!=EJson::Object || !Model.Value->AsObject()->TryGetObjectField(TEXT("textures"),Textures)) continue;
        TArray<FString> Ids; (*Textures)->Values.GetKeys(Ids); Ids.Sort();
        Entries.Add(Model.Key+TEXT(": ")+FString::Join(Ids,TEXT("/")));
    }
    Entries.Sort(); return FString::Join(Entries,TEXT("、"));
}
bool FEquipmentLibrary::ResolveTexture(const FString& ModelId,const FString& Type,const FString& Side,const FString& TextureId,UTexture*& Texture,UMaterialInterface*& Material,int32& Slot,FString& Error)
{
    Texture=nullptr; Material=nullptr; Slot=0; Error.Reset();
    if(TextureId.IsEmpty()) return true;
    const FEquipmentDefinition* Definition=Presentation(ModelId,Type,Side);
    const FString* Asset=Definition?Definition->Textures.Find(TextureId):nullptr;
    if(!Asset || Definition->TextureMaterial.IsEmpty())
    { Error=TEXT("该模型没有登记贴图：")+ModelId+TEXT(" / ")+TextureId; return false; }
    if(const auto* Loaded=LoadedTextures.Find(*Asset)) Texture=Loaded->Get();
    else if((Texture=LoadObject<UTexture>(nullptr,**Asset,nullptr,LOAD_NoWarn))) LoadedTextures.Add(*Asset,TStrongObjectPtr<UTexture>(Texture));
    if(const auto* Loaded=TextureMaterials.Find(Definition->TextureMaterial)) Material=Loaded->Get();
    else if((Material=LoadObject<UMaterialInterface>(nullptr,*Definition->TextureMaterial,nullptr,LOAD_NoWarn))) TextureMaterials.Add(Definition->TextureMaterial,TStrongObjectPtr<UMaterialInterface>(Material));
    FMaterialParameterInfo Parameter(TEXT("AircraftDiffuse")); UTexture* DefaultTexture=nullptr;
    if(!Texture || !Material || !Material->GetTextureParameterValue(Parameter,DefaultTexture))
    { Error=TEXT("贴图资源或可切换材质无效：")+ModelId+TEXT(" / ")+TextureId; return false; }
    Slot=Definition->TextureSlot; return true;
}

UStaticMesh* FEquipmentLibrary::LoadMesh(const FString& Id)
{
    if (const auto* Existing = Meshes.Find(Id)) return Existing->Get();
    const FEquipmentDefinition* Definition = Models.Find(Id);
    if (!Definition) return nullptr;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Definition->Asset, nullptr, LOAD_NoWarn);
    if (Mesh) Meshes.Add(Id, TStrongObjectPtr<UStaticMesh>(Mesh));
    return Mesh;
}

UStaticMesh* FEquipmentLibrary::Resolve(const FString& ModelId, const FString& Type, const FString& Side, FString& OutError, bool bRequireExactModel,FString* ResolvedModelId)
{
    OutError.Reset();
    if(ResolvedModelId) ResolvedModelId->Reset();
    FString EffectiveId=ModelId;
    const FString TypeDefault=Type+TEXT("_default");
    if(ModelId.IsEmpty() || ModelId==TypeDefault)
        if(const FString* SideId=SideDefaults.Find(Type+TEXT(".")+Side)) EffectiveId=*SideId;
    const FEquipmentDefinition* Definition = Models.Find(EffectiveId);
    if (Definition && Definition->Type == Type)
    {
        if (UStaticMesh* Mesh = LoadMesh(EffectiveId)) { if(ResolvedModelId) *ResolvedModelId=EffectiveId; return Mesh; }
        if (bRequireExactModel)
        {
            OutError = TEXT("无法加载注册模型资源：") + EffectiveId;
            return nullptr;
        }
    }
    else if (bRequireExactModel)
    {
        OutError = TEXT("模型未登记或类型不匹配：") + EffectiveId + TEXT(" (") + Type + TEXT(")");
        return nullptr;
    }
    // A broken explicit model falls back to the stable type default. Side defaults
    // are used for normal implicit selection above, but do not hide bad asset ids.
    const FString* DefaultId = Defaults.Find(Type);
    UStaticMesh* Fallback = DefaultId ? LoadMesh(*DefaultId) : nullptr;
    if (!Fallback)
    {
        OutError = TEXT("No usable default model for type: ") + Type;
        return nullptr;
    }
    UE_LOG(LogTemp, Warning, TEXT("[TacticalDemo] modelId '%s' unavailable or wrong type; using '%s' for %s"), *ModelId, **DefaultId, *Side);
    if(ResolvedModelId) *ResolvedModelId=*DefaultId;
    return Fallback;
}
const FEquipmentDefinition* FEquipmentLibrary::Presentation(const FString& ModelId,const FString& Type,const FString& Side) const
{
    FString Id=ModelId;
    if(Id.IsEmpty() || Id==Type+TEXT("_default"))
        if(const FString* SideId=SideDefaults.Find(Type+TEXT(".")+Side)) Id=*SideId;
    const FEquipmentDefinition* Definition=Models.Find(Id);
    if(Definition && Definition->Type==Type) return Definition;
    const FString* Default=Defaults.Find(Type);
    return Default?Models.Find(*Default):nullptr;
}
FString FEquipmentLibrary::EffectiveModelId(const FString& ModelId,const FString& Type,const FString& Side) const
{
    if(ModelId.IsEmpty() || ModelId==Type+TEXT("_default"))
        if(const FString* Id=SideDefaults.Find(Type+TEXT(".")+Side)) return *Id;
    return ModelId;
}
bool FEquipmentLibrary::TextureModelSelector(const FString& User,FString& ModelId,FString& Error) const
{
    ModelId.Reset();
    for(const auto& Model:Models)
    {
        if(Model.Value.Textures.IsEmpty() || !User.Contains(Model.Key,ESearchCase::IgnoreCase)) continue;
        if(!ModelId.IsEmpty()) { Error=TEXT("请按机型分别切换贴图，或使用全部飞机作为范围。"); return false; }
        ModelId=Model.Key;
    }
    return true;
}
