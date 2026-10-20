#include <fbxsdk.h>
#include <fstream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>
#include <iostream>

static std::string json(const char* s) {
    std::string out="\"";
    for (const unsigned char c:std::string(s)) {
        if(c=='"' || c=='\\') out+='\\';
        if(c>=32) out+=c;
    }
    return out+"\"";
}
template<class V> static void vector(std::ostream& o,const V& v,int n=3) {
    o<<'['; for(int i=0;i<n;++i) {if(i)o<<','; o<<v[i];} o<<']';
}
int main(int argc,char**argv) {
    if(argc!=5) {std::cerr<<"source.fbx report.json selected-names.txt filtered.fbx\n";return 2;}
    std::set<std::string> selected; std::ifstream list(argv[3]); std::string line;
    while(std::getline(list,line)) {if(!line.empty() && line.back()=='\r') line.pop_back(); if(!line.empty()) selected.insert(line);}
    auto* manager=FbxManager::Create(); auto* settings=FbxIOSettings::Create(manager,IOSROOT); manager->SetIOSettings(settings);
    auto* scene=FbxScene::Create(manager,"Map3"); auto* importer=FbxImporter::Create(manager,"");
    if(!importer->Initialize(argv[1],-1,settings)||!importer->Import(scene)) {std::cerr<<importer->GetStatus().GetErrorString();return 3;}
    importer->Destroy();
    FbxAxisSystem axis(FbxAxisSystem::eZAxis,(FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd,FbxAxisSystem::eRightHanded);
    axis.ConvertScene(scene); FbxSystemUnit::cm.ConvertScene(scene);
    std::ofstream report(argv[2]); report<<std::setprecision(15)<<"[\n"; bool first=true;
    std::vector<FbxNode*> nodes;
    for(int i=0;i<scene->GetNodeCount();++i) nodes.push_back(scene->GetNode(i));
    int retained=0;
    for(auto* node:nodes) {
        auto* mesh=node->GetMesh(); if(!mesh) continue;
        FbxAMatrix geometry; geometry.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
        geometry.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot)); geometry.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));
        auto matrix=node->EvaluateGlobalTransform()*geometry;
        auto p=matrix.GetT();p[1]=-p[1]; auto q=matrix.GetQ();q[1]=-q[1];q[3]=-q[3];auto s=matrix.GetS();
        FbxVector4 lo(1e30,1e30,1e30),hi(-1e30,-1e30,-1e30),wlo=lo,whi=hi;
        for(int v=0;v<mesh->GetControlPointsCount();++v) {
            auto local=mesh->GetControlPointAt(v);auto world=matrix.MultT(local); local[1]=-local[1];world[1]=-world[1];
            for(int a=0;a<3;++a){lo[a]=std::min(lo[a],local[a]);hi[a]=std::max(hi[a],local[a]);wlo[a]=std::min(wlo[a],world[a]);whi[a]=std::max(whi[a],world[a]);}
        }
        if(!first)report<<",\n";first=false;
        report<<"{\"name\":"<<json(node->GetName())<<",\"position\":";vector(report,p);
        report<<",\"quaternion\":";vector(report,q,4);report<<",\"scale\":";vector(report,s);
        report<<",\"localMin\":";vector(report,lo);report<<",\"localMax\":";vector(report,hi);
        report<<",\"worldMin\":";vector(report,wlo);report<<",\"worldMax\":";vector(report,whi);
        report<<",\"vertices\":"<<mesh->GetControlPointsCount()<<",\"materials\":[";
        for(int i=0;i<node->GetMaterialCount();++i){if(i)report<<',';report<<json(node->GetMaterial(i)->GetName());}
        report<<"]}";
        if(!selected.count(node->GetName())) {node->RemoveNodeAttribute(mesh);} else ++retained;
    }
    report<<"\n]\n";report.close();
    for(int i=scene->GetGeometryCount()-1;i>=0;--i) {
        auto* geometry=scene->GetGeometry(i);
        if(geometry->GetNodeCount()==0) geometry->Destroy();
    }
    // Export only selected new environmental geometry; keep transform hierarchy.
    // Source FBX and existing UE resources are never modified.
    auto* exporter=FbxExporter::Create(manager,"");
    if(!exporter->Initialize(argv[4],-1,settings)||!exporter->Export(scene)) {std::cerr<<exporter->GetStatus().GetErrorString();return 4;}
    exporter->Destroy();manager->Destroy();std::cout<<"Retained "<<retained<<" environmental meshes\n";
}
