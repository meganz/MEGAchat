def failedTargets = []

pipeline {
    agent { label 'osx && arm64' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'BUILD_XFRAMEWORK', defaultValue: false, description: 'Build SDK/MEGAchat xframework for iOS')
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        string(name: 'IOS_BRANCH', defaultValue: '', description: 'Branch of iOS-dev used to upload xframework')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                sh "echo Cloning MEGAchat branch \"${params.MEGACHAT_BRANCH}\""
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "${params.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${params.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${params.SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }


        stage('Build iOS'){
            options{
                timeout(time: 120, unit: 'MINUTES')
            }
            environment{
                PATH = "/usr/local/bin:${env.PATH}"
                VCPKGPATH = "${env.HOME}/jenkins/vcpkg"
                BUILD_DIR_ARM64 = "build_dir_arm64"
                BUILD_DIR_ARM64_SIM = "build_dir_arm64_simulator"
                VCPKG_BINARY_SOURCES = 'clear;x-aws,s3://vcpkg-cache/archives/,readwrite'
                AWS_ACCESS_KEY_ID = credentials('s4_access_key_id_vcpkg_cache')
                AWS_SECRET_ACCESS_KEY = credentials('s4_secret_access_key_vcpkg_cache')
                AWS_ENDPOINT_URL = "https://s3.g.s4.mega.io"
            }
            steps{
                //Build MEGAchat for arm64
                sh "echo Building MEGAchat for iOS arm64"
                sh "cmake --preset mega-ios -DVCPKG_TARGET_TRIPLET=arm64-ios-mega -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVCPKG_ROOT=${VCPKGPATH} -DCMAKE_VERBOSE_MAKEFILE=ON -S ${WORKSPACE} -B ${WORKSPACE}/${BUILD_DIR_ARM64}"
                sh "cmake --build ${WORKSPACE}/${BUILD_DIR_ARM64} -j2"

                //Build MEGAchat for arm64 simulator
                sh "echo \"Building MEGAchat for iOS arm64 simulator (crosscompiling)\""
                sh "cmake --preset mega-ios -DVCPKG_TARGET_TRIPLET=arm64-ios-simulator-mega -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVCPKG_ROOT=${VCPKGPATH} -DCMAKE_VERBOSE_MAKEFILE=ON -S ${WORKSPACE} -B ${WORKSPACE}/${BUILD_DIR_ARM64_SIM}"
                sh "cmake --build ${WORKSPACE}/${BUILD_DIR_ARM64_SIM} -j2"
            }
        }
        stage ("Build and upload iOS xframework") {
            when { expression { return  params.BUILD_XFRAMEWORK } }
            environment {
                BUILD_DIR_ARM64 = "build_dir_arm64"
                BUILD_DIR_ARM64_SIM = "build_dir_arm64_simulator"
                IOS_DIR="ios"

                // These are the directories where build-sdk-libs.sh expects the libs being built
                BUILD_DIR_DEVICE="BUILD_ARM64_iOS"
                BUILD_DIR_SIMULATOR="BUILD_ARM64_simulator"
            }
            steps {
                echo "IOS_BRANCH: ${IOS_BRANCH}"
                dir("${IOS_DIR}"){
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${params.IOS_BRANCH}"  ]],
                        userRemoteConfigs: [[ url: "${GIT_URL_IOS}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                    sh """
                        # Link built libraries
                        rm -rf ${BUILD_DIR_DEVICE}
                        rm -rf ${BUILD_DIR_SIMULATOR}
                        ln -s ${WORKSPACE}/${BUILD_DIR_ARM64} ${BUILD_DIR_DEVICE}
                        ln -s ${WORKSPACE}/${BUILD_DIR_ARM64_SIM} ${BUILD_DIR_SIMULATOR}

                        # Link SDK and MEGAchat
                        rm -rf Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK
                        rm -rf Modules/DataSource/MEGASDK/Sources/MEGASDK
                        ln -s ${megachat_sources_workspace} Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK
                        ln -s ${sdk_sources_workspace} Modules/DataSource/MEGASDK/Sources/MEGASDK

                        # Export the right path
                        export PATH="\$(xcode-select -p)/Toolchains/XcodeDefault.xctoolchain/usr/bin:\$PATH"
                        bash -x scripts/build-sdk-libs.sh --skip-build-libs
                    """
                }
            }
            post {
                always {
                   archiveArtifacts allowEmptyArchive: false, artifacts: "${IOS_DIR}/xcframework/"
                   archiveArtifacts allowEmptyArchive: true, artifacts: "${IOS_DIR}/${BUILD_DIR_DEVICE}"
                   archiveArtifacts allowEmptyArchive: true, artifacts: "${IOS_DIR}/${BUILD_DIR_SIMULATOR}"
                }
            }
        }
    }
    post {
        always {
            script {
                if (params.RESULT_TO_SLACK) {
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    def message = """
                        *MEGAchat iOS* <${BUILD_URL}|Build result>: '${messageStatus}'.
                        MEGAchat branch: `${MEGACHAT_BRANCH}`
                        MEGAchat commit: `${megachat_commit}`
                        SDK branch: `${SDK_BRANCH}`
                        SDK commit: `${sdk_commit}`
                    """.stripIndent()

                    withCredentials([string(credentialsId: 'slack_webhook_sdk_report', variable: 'SLACK_WEBHOOK_URL')]) {
                        sh """
                            curl -X POST -H 'Content-type: application/json' --data '
                                {
                                "attachments": [
                                    {
                                        "color": "${messageColor}",
                                        "blocks": [
                                        {
                                            "type": "section",
                                            "text": {
                                                    "type": "mrkdwn",
                                                    "text": "${message}"
                                            }
                                        }
                                        ]
                                    }
                                    ]
                                }' \${SLACK_WEBHOOK_URL}
                         """
                    }
                }
            }
            deleteDir() /* clean up our workspace */
        }
    }
}
