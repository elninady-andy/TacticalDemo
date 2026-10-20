#include <fbxsdk.h>
#include <iostream>
#include <string>
#include <vector>

// Preserve the exported aircraft orientation/scale, but remove its world position.
// This produces reusable FBX models rather than meshes offset by hundreds of metres.
int main(int argc, char** argv) {
    if(argc!=3) {std::cerr<<"source.fbx output-directory\n";return 2;}
    for(const std::string kind: {"J20_Map3", "F35"}) {
        auto* manager=FbxManager::Create();
        auto* settings=FbxIOSettings::Create(manager, IOSROOT); manager->SetIOSettings(settings);
        auto* scene=FbxScene::Create(manager,"Aircraft"); auto* importer=FbxImporter::Create(manager,"");
        if(!importer->Initialize(argv[1],-1,settings)||!importer->Import(scene)) {std::cerr<<importer->GetStatus().GetErrorString();return 3;}
        importer->Destroy();
        FbxAxisSystem axis(FbxAxisSystem::eZAxis,(FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd,FbxAxisSystem::eRightHanded);
        axis.ConvertScene(scene); FbxSystemUnit::cm.ConvertScene(scene);
        auto* root=scene->FindNodeByName(kind=="F35"?"BP_F_35":"BP_J20S2");
        if(!root) {std::cerr<<"Aircraft root missing "<<kind;return 4;}
        const auto transform=root->EvaluateGlobalTransform();
        if(root->GetParent()!=scene->GetRootNode()) scene->GetRootNode()->AddChild(root);
        root->LclTranslation.Set(FbxDouble3(0,0,0)); root->LclRotation.Set(transform.GetR()); root->LclScaling.Set(transform.GetS());
        std::vector<FbxNode*> nodes;
        for(int i=0;i<scene->GetNodeCount();++i) nodes.push_back(scene->GetNode(i));
        int retained=0;
        for(auto* node:nodes) {
            auto* mesh=node->GetMesh(); if(!mesh)continue;
            const std::string name=node->GetName();
            bool keep=kind=="F35"?name=="BP_F_35":name.rfind("J20S_",0)==0;
            if(keep) ++retained; else node->RemoveNodeAttribute(mesh);
        }
        for(int i=scene->GetGeometryCount()-1;i>=0;--i) if(scene->GetGeometry(i)->GetNodeCount()==0) scene->GetGeometry(i)->Destroy();
        if(retained!=(kind=="F35"?1:12)) {std::cerr<<"Unexpected geometry count "<<retained;return 5;}
        auto* exporter=FbxExporter::Create(manager,"");
        const auto path=std::string(argv[2])+"/"+kind+".fbx";
        if(!exporter->Initialize(path.c_str(),-1,settings)||!exporter->Export(scene)) {std::cerr<<exporter->GetStatus().GetErrorString();return 6;}
        std::cout<<kind<<" retained="<<retained<<" original position="<<transform.GetT()[0]<<","<<transform.GetT()[1]<<","<<transform.GetT()[2]<<"\n";
        exporter->Destroy(); manager->Destroy();
    }
}
