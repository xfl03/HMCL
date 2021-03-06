/**
 * Hello Minecraft! Launcher
 * Copyright (C) 2018  huangyuhui <huanghongxun2008@126.com> and contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
package org.jackhuang.hmcl.ui.wizard;

import javafx.scene.Node;

import java.util.Map;
import java.util.function.Function;

public class SinglePageWizardProvider implements WizardProvider {

    private final Function<WizardController, WizardSinglePage> provider;
    private WizardSinglePage page;

    public SinglePageWizardProvider(Function<WizardController, WizardSinglePage> provider) {
        this.provider = provider;
    }

    @Override
    public void start(Map<String, Object> settings) {
    }

    @Override
    public Object finish(Map<String, Object> settings) {
        return page.finish();
    }

    @Override
    public Node createPage(WizardController controller, int step, Map<String, Object> settings) {
        if (step != 0) throw new IllegalStateException("Step must be 0");

        return page = provider.apply(controller);
    }

    @Override
    public boolean cancel() {
        return true;
    }
}
