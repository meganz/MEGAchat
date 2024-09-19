pipeline {
    agent { label 'osx && arm64' }
    options {
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
        ansiColor('xterm')
    }
    environment {
        APIURL_TO_TEST = "https://g.api.mega.co.nz/"
        IOS_BRANCH = "develop"
        SDK_BRANCH = "develop"
        MEGACHAT_BRANCH = "develop"
    }
    stages {
        stage('Checkout SDK MEGAchat and iOS'){
            steps {
                deleteDir()
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "origin/${env.IOS_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:mobile/ios/iOS_dev.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                dir("Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK"){
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${env.MEGACHAT_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"],
                        ]
                    ])
                }
                dir('Modules/Datasource/MEGASDK/Sources/MEGASDK'){
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${env.SDK_BRANCH}"]],
                            userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                            extensions: [
                                [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                            ]
                    ])
                }
                script{
                    ios_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${ios_sources_workspace}/Modules/DataSource/MEGASDK/Sources/MEGASDK"
                    megachat_sources_workspace = "${ios_sources_workspace}/Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK"
                }
            }
        }
        stage('Build MEGACHAT SDK and iOS'){
            environment{
                PATH = "/usr/local/bin:${env.PATH}"
                LIBTOOLIZE = "/usr/local/bin/glibtoolize"
            }
            steps{
                sh """
                    bundle config set --local path 'vendor/bundle'
                    bundle install
                """
                dir("${megachat_sources_workspace}/src"){
                    sh """
                        cmake -P genDbSchema.cmake
                    """
                }
                sh """
                    bundle exec fastlane configure_sdk_and_chat_library use_cache:true
                """
                withCredentials([gitUsernamePassword(credentialsId: 'jenkins_sdk_token_with_user', gitToolName: 'Default')]) {
                    sh """
                        bundle exec fastlane update_plugins
                        bundle exec fastlane build_simulator
                    """
                }
            }
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
