// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/insets.h"

namespace ash {

namespace {

typedef test::AshTestBase WorkspaceLayoutManagerTest;

// Verifies that a window containing a restore coordinate will be restored to
// to the size prior to minimize, keeping the restore rectangle in tact (if
// there is one).
TEST_F(WorkspaceLayoutManagerTest, RestoreFromMinimizeKeepsRestore) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);
  // This will not be used for un-minimizing window.
  SetRestoreBoundsInScreen(window.get(), gfx::Rect(0, 0, 100, 100));
  wm::MinimizeWindow(window.get());
  wm::RestoreWindow(window.get());
  EXPECT_EQ("0,0 100x100", GetRestoreBoundsInScreen(window.get())->ToString());
  EXPECT_EQ("10,15 25x35", window.get()->bounds().ToString());

  UpdateDisplay("400x300,500x400");
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenAsh::GetSecondaryDisplay());
  EXPECT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());
  wm::MinimizeWindow(window.get());
  // This will not be used for un-minimizing window.
  SetRestoreBoundsInScreen(window.get(), gfx::Rect(0, 0, 100, 100));
  wm::RestoreWindow(window.get());
  EXPECT_EQ("600,0 100x100", window->GetBoundsInScreen().ToString());

  // Make sure the unminimized window moves inside the display when
  // 2nd display is disconnected.
  wm::MinimizeWindow(window.get());
  UpdateDisplay("400x300");
  wm::RestoreWindow(window.get());
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), window->GetRootWindow());
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
}

// WindowObserver implementation used by DontClobberRestoreBoundsWindowObserver.
// This code mirrors what BrowserFrameAura does. In particular when this code
// sees the window was maximized it changes the bounds of a secondary
// window. The secondary window mirrors the status window.
class DontClobberRestoreBoundsWindowObserver : public aura::WindowObserver {
 public:
  DontClobberRestoreBoundsWindowObserver() : window_(NULL) {}

  void set_window(aura::Window* window) { window_ = window; }

  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    if (!window_)
      return;

    if (wm::IsWindowMaximized(window)) {
      aura::Window* w = window_;
      window_ = NULL;

      gfx::Rect shelf_bounds(Shell::GetPrimaryRootWindowController()->
                             GetShelfLayoutManager()->GetIdealBounds());
      const gfx::Rect& window_bounds(w->bounds());
      w->SetBounds(gfx::Rect(window_bounds.x(), shelf_bounds.y() - 1,
                             window_bounds.width(), window_bounds.height()));
    }
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DontClobberRestoreBoundsWindowObserver);
};

// Creates a window, maximized the window and from within the maximized
// notification sets the bounds of a window to overlap the shelf. Verifies this
// doesn't effect the restore bounds.
TEST_F(WorkspaceLayoutManagerTest, DontClobberRestoreBounds) {
  DontClobberRestoreBoundsWindowObserver window_observer;
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 20, 30, 40));
  // NOTE: for this test to exercise the failure the observer needs to be added
  // before the parent set. This mimics what BrowserFrameAura does.
  window->AddObserver(&window_observer);
  SetDefaultParentByPrimaryRootWindow(window.get());
  window->Show();
  ash::wm::ActivateWindow(window.get());

  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 30, 40)));
  window->AddTransientChild(window2.get());
  window2->Show();

  window_observer.set_window(window2.get());
  wm::MaximizeWindow(window.get());
  EXPECT_EQ("10,20 30x40", GetRestoreBoundsInScreen(window.get())->ToString());
  window->RemoveObserver(&window_observer);
}

// Verifies when a window is maximized all descendant windows have a size.
TEST_F(WorkspaceLayoutManagerTest, ChildBoundsResetOnMaximize) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 30, 40)));
  window->Show();
  ash::wm::ActivateWindow(window.get());
  scoped_ptr<aura::Window> child_window(
      aura::test::CreateTestWindowWithBounds(gfx::Rect(5, 6, 7, 8),
                                             window.get()));
  child_window->Show();
  ash::wm::MaximizeWindow(window.get());
  EXPECT_EQ("5,6 7x8", child_window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, WindowShouldBeOnScreenWhenAdded) {
  // Normal window bounds shouldn't be changed.
  gfx::Rect window_bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds, window->bounds());

  // If the window is out of the workspace, it would be moved on screen.
  gfx::Rect root_window_bounds =
      ash::Shell::GetInstance()->GetPrimaryRootWindow()->bounds();
  window_bounds.Offset(root_window_bounds.width(), root_window_bounds.height());
  ASSERT_FALSE(window_bounds.Intersects(root_window_bounds));
  scoped_ptr<aura::Window> out_window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds.size(), out_window->bounds().size());
  gfx::Rect bounds = out_window->bounds();
  bounds.Intersect(root_window_bounds);
  // 2/3 of the window must be visible.
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.6);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.6);
}

// Verifies the size of a window is enforced to be smaller than the work area.
TEST_F(WorkspaceLayoutManagerTest, SizeToWorkArea) {
  // Normal window bounds shouldn't be changed.
  gfx::Size work_area(
      Shell::GetScreen()->GetPrimaryDisplay().work_area().size());
  const gfx::Rect window_bounds(
      100, 101, work_area.width() + 1, work_area.height() + 2);
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
      window->bounds().ToString());

  // Directly setting the bounds triggers a slightly different code path. Verify
  // that too.
  window->SetBounds(window_bounds);
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
      window->bounds().ToString());
}

}  // namespace
}  // namespace ash
