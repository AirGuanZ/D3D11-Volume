#include <agz-utils/string.h>

#include "display.h"
#include "envir.h"
#include "raw.h"
#include "volume.h"

class ReSTIRVolumeDemo : public Demo
{
public:

    using Demo::Demo;

private:

    Camera camera_;

    Displayer         disp_;
    RawVolumeRenderer raw_;

    EnvirLight envir_;
    Volume     volume_;

    bool discardHistory_ = false;

    float exposure_ = 1;
    
    float envirIntensity_ = 1;

    float  densityScale_ = 10;
    float  g_ = 0;
    Float3 lower_ = { -1, -1, -1 };
    Float3 upper_ = { 1, 1, 1 };

    int maxDepth_ = 5;

    ImGui::FileBrowser fileBrowser_;

    void initialize() override
    {
        disp_.initialize();
        raw_.initilalize(window_->getClientSize());

        envir_.initialize("./asset/sky.hdr", { 200, 200 });

        volume_.initialize();
        volume_.loadAlbedo("./asset/albedo.txt");
        volume_.loadDensity("./asset/density.txt");

        window_->attach([&](const WindowPostResizeEvent &e)
        {
            raw_.resize({ e.width, e.height });
        });

        const Float3 extent = Float3(1.98f, 1.98f, 0.78f);
        lower_ = -extent;
        upper_ = extent;

        fileBrowser_.SetTitle("Select Envir Light");
        fileBrowser_.SetTypeFilters({ ".hdr"});

        camera_.setPosition(Float3(0, 0, -4));
        camera_.setDirection(3.1415926f / 2, 0);
        camera_.setPerspective(60.0f, 0.1f, 100.0f);

        mouse_->setCursorLock(true, window_->getClientSize() / 2);
        mouse_->showCursor(false);

        window_->doEvents();
    }

    void frame() override
    {
        if(keyboard_->isDown(KEY_ESCAPE))
            window_->setCloseFlag(true);

        if(keyboard_->isDown(KEY_LCTRL))
        {
            mouse_->showCursor(!mouse_->isVisible());
            mouse_->setCursorLock(!mouse_->isLocked(), mouse_->getLockXY());
        }

        if(ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            discardHistory_ |= ImGui::InputFloat("Envir Intensity", &envirIntensity_);
            discardHistory_ |= ImGui::InputFloat("Density Scale", &densityScale_);
            discardHistory_ |= ImGui::SliderFloat("g", &g_, -0.99f, 0.99f);
            discardHistory_ |= ImGui::InputInt("Max Depth", &maxDepth_);

            ImGui::InputFloat("Exposure", &exposure_);

            if(ImGui::Button("Envir Light"))
                fileBrowser_.Open();
        }
        ImGui::End();

        fileBrowser_.Display();
        if(fileBrowser_.HasSelected())
        {
            const auto filename = fileBrowser_.GetSelected().string();
            fileBrowser_.ClearSelected();
            envir_.initialize(filename, { 200, 200 });
        }

        if(keyboard_->isPressed('W') ||
           keyboard_->isPressed('A') ||
           keyboard_->isPressed('D') ||
           keyboard_->isPressed('S') ||
           keyboard_->isPressed(KEY_SPACE) ||
           keyboard_->isPressed(KEY_LSHIFT))
            window_->setVSync(true);
        else
            window_->setVSync(false);
        
        camera_.setWOverH(window_->getClientWOverH());
        if(!mouse_->isVisible())
        {
            camera_.update({
                .front      = keyboard_->isPressed('W'),
                .left       = keyboard_->isPressed('A'),
                .right      = keyboard_->isPressed('D'),
                .back       = keyboard_->isPressed('S'),
                .up         = keyboard_->isPressed(KEY_SPACE),
                .down       = keyboard_->isPressed(KEY_LSHIFT),
                .cursorRelX = static_cast<float>(mouse_->getRelativePositionX()),
                .cursorRelY = static_cast<float>(mouse_->getRelativePositionY())
            });
        }
        camera_.recalculateMatrics();

        envir_.updateConstantBuffer(envirIntensity_);

        volume_.setBoundingBox(lower_, upper_);
        volume_.setDensityScale(densityScale_);
        volume_.setG(g_);
        volume_.updateConstantBuffer();

        if(discardHistory_)
        {
            raw_.discardHistory();
            discardHistory_ = false;
        }
        raw_.setCamera(camera_);
        raw_.setEnvir(envir_);
        raw_.setVolume(volume_);
        raw_.setTracer(maxDepth_);
        
        raw_.render();

        window_->useDefaultRTVAndDSV();
        window_->useDefaultViewport();
        window_->clearDefaultDepth(1);
        window_->clearDefaultRenderTarget({ 0, 1, 1, 0 });

        disp_.setExposure(exposure_);
        disp_.render(raw_.getOutput());
    }

    std::vector<std::string> getSortedImages(const std::string &path) const
    {
        const auto isPic = [](const std::string &lext)
        {
            return lext == ".jpg"  || lext == ".png" ||
                   lext == ".jpeg" || lext == ".bmp";
        };

        std::vector<std::string> filenames;
        for(auto &entry : std::filesystem::directory_iterator(path))
        {
            const auto p = entry.path();
            const auto ext = p.extension();
            if(isPic(agz::stdstr::to_lower(ext.string())))
                filenames.push_back(p.string());
        }

        std::ranges::sort(filenames);
        return filenames;
    }
};

int main()
{
    ReSTIRVolumeDemo(WindowDesc{
        .clientSize = { 640, 480 },
        .title      = L"D3D11 Volume Renderer",
        .vsync      = false
    }).run();
}
