/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
import { describe, beforeAll, beforeEach, afterEach, afterAll} from 'deccjsunit/index'
export default function FaultloggedTest() {

describe("FaultloggedTest", function () {
    const waitMillSecond = 3000;
    const testFuncLayer = 5;

    function jsFunc(layer) {
        if (layer > 0) {
            jsFunc(layer - 1);
        } else {
            let startTime = new Date().getTime();
            while (new Date().getTime() -  startTime < waitMillSecond);
        }
    }

    beforeAll(function() {
        console.info('FaultloggedTest beforeAll called')
        jsFunc(testFuncLayer);
    })

    afterAll(function() {
        console.info('FaultloggedTest afterAll called')
    })

    beforeEach(function() {
        console.info('FaultloggedTest beforeEach called')
    })

    afterEach(function() {
        console.info('FaultloggedTest afterEach called')
    })

})
}
